#!/usr/bin/env python3
"""
Validates a Clees4All VICS JSON export (VICS_Pictures_Results.json /
VICS_Movies_Results.json) against a file list exported from X-Ways,
checking that every MediaFiles entry in the JSON has a matching row in the
export with a consistent MD5 hash, file size, and MAC timestamps.

Defaults match X-Ways' native file list export: tab-delimited, UTF-16,
with an 'Evidence object' and a 'Hash¹' column. Use --csv-delimiter/
--csv-encoding/--col-* to override if your export differs (e.g. a
comma-separated, UTF-8 CSV from a different tool).

Built for multi-gigabyte inputs:
  - The file list is streamed row-by-row into a disposable on-disk SQLite
    index (never held fully in memory), keyed on a normalised MD5+path
    string.
  - The JSON is parsed incrementally with ijson, so only one Media record
    (plus its MediaFiles) is ever materialised in memory at a time.
  - Each evidence object's true UTC offset (including cases where it
    differs by season, e.g. GMT/BST) is derived automatically from
    identity-matched files rather than requiring it as an argument.

Requires: pip install ijson

Example:
    python validate_vics_json.py \
        --csv "Export Case Root.txt" \
        --json VICS_Pictures_Results.json --json VICS_Movies_Results.json \
        --report mismatches.csv --orphan-report csv_only.csv
"""

import argparse
import csv
import os
import re
import sqlite3
import sys
import tempfile
import time
from datetime import datetime, timezone, timedelta

try:
    import ijson
except ImportError:
    sys.exit("This script requires the 'ijson' package. Install it with: pip install ijson")

try:
    import ijson.backends.yajl2_c as _ijson_fast
    IJSON = _ijson_fast
except ImportError:
    IJSON = ijson

CSV_TIMESTAMP_FORMATS = [
    '%Y-%m-%d %H:%M:%S',
    '%Y-%m-%dT%H:%M:%S',
    '%d/%m/%Y %H:%M:%S',
    '%m/%d/%Y %H:%M:%S',
    '%d.%m.%Y %H:%M:%S',
    '%Y-%m-%d %H:%M',
    '%d/%m/%Y %H:%M',
    '%d/%m/%Y',
]


# ---------------------------------------------------------------------------
# Normalisation helpers
# ---------------------------------------------------------------------------

def normalize_path_key(*parts):
    """Splits every part on \\ or /, drops empty/whitespace segments, and
    rejoins lowercased - so differences in leading/trailing/doubled
    separators between the JSON's reconstructed path and the CSV's exported
    path column don't cause false mismatches."""
    segments = []
    for part in parts:
        if not part:
            continue
        for seg in re.split(r'[\\/]+', str(part)):
            seg = seg.strip()
            if seg:
                segments.append(seg.lower())
    return '\\'.join(segments)


def strip_leading_segment(path):
    """Drops the first path segment. The JSON's FilePath begins with a
    partition/evidence placeholder (e.g. 'Partition 1\\...') that has no
    stable, textually-matching counterpart in the CSV's Evidence Object
    column (e.g. '006620-26_DG-03, P1') - the two exports label evidence
    roots differently, so that segment is excluded from the match key on
    both sides rather than compared."""
    segs = [s for s in re.split(r'[\\/]+', path or '') if s.strip()]
    return '\\'.join(segs[1:]) if len(segs) > 1 else ''


def strip_extraction_annotation(name):
    """X-Ways appends ' [annotation]' to the Name of files it extracted or
    carved out of a container: embedded objects (e.g. a picture inside a
    PDF) get an empty ' []', while other recovered objects (e.g. browser
    cache entries) get their original/derived name in brackets, e.g.
    'f_00621d [SD_01_T97_3222E_Y4_X_EC_90]'. The VICS JSON's FileName never
    carries this annotation, so it's stripped before building the match key
    - the raw CSV name (with annotation) is still stored/reported as-is."""
    return re.sub(r'\s\[[^\[\]]*\]$', '', name)


def normalize_md5(value):
    if not value:
        return ''
    return str(value).strip().lower()


def parse_int(value):
    if value is None:
        return None
    v = str(value).strip().replace(',', '').replace(' ', '')
    if v == '':
        return None
    try:
        return int(v)
    except ValueError:
        return None


def parse_iso_timestamp(value):
    """Parses the ISO-8601 UTC timestamps written by the plugin's
    cjsonAddFiletime, e.g. '2024-01-02T03:04:05.678Z' or '...+02:00'."""
    if not value:
        return None
    v = value.strip()
    if v.endswith('Z'):
        v = v[:-1] + '+00:00'
    try:
        dt = datetime.fromisoformat(v)
        if dt.tzinfo is None:
            dt = dt.replace(tzinfo=timezone.utc)
        return dt.astimezone(timezone.utc)
    except ValueError:
        return None


CSV_TIMESTAMP_FORMATS_HAS_TIME = [True, True, True, True, True, True, True, False]


def parse_csv_timestamp_naive(value):
    """Parses a CSV timestamp WITHOUT attaching any time zone - the raw
    value is local wall-clock time in whatever zone X-Ways was configured to
    display when the CSV was exported, and that zone can differ per
    evidence object (see calibrate_evidence_offsets), so it's resolved to
    UTC later, not here.

    Returns (naive_datetime, has_time_component) or (None, False). The
    has_time flag distinguishes a genuine date-only export value (e.g. an
    'Accessed' column showing just '03/04/2022') from a full timestamp, so
    callers can compare it as a calendar date instead of an exact instant."""
    if not value:
        return None, False
    # Collapse runs of whitespace (some X-Ways exports pad the date/time
    # separator with two spaces instead of one) so a single set of formats
    # above covers both.
    v = re.sub(r'\s+', ' ', value.strip())
    if not v:
        return None, False
    for fmt, has_time in zip(CSV_TIMESTAMP_FORMATS, CSV_TIMESTAMP_FORMATS_HAS_TIME):
        try:
            return datetime.strptime(v, fmt), has_time
        except ValueError:
            continue
    return None, False


def localize_to_utc(naive_dt, tz_offset_hours):
    if naive_dt is None:
        return None
    return naive_dt.replace(tzinfo=timezone(timedelta(hours=tz_offset_hours))).astimezone(timezone.utc)


def timestamps_match(json_dt, csv_dt, tolerance_seconds):
    if json_dt is None and csv_dt is None:
        return True
    if json_dt is None or csv_dt is None:
        return False
    return abs((json_dt - csv_dt).total_seconds()) <= tolerance_seconds


# ---------------------------------------------------------------------------
# Stage 1: stream the CSV into a scratch SQLite index
# ---------------------------------------------------------------------------

def build_csv_index(csv_path, db_path, columns, delimiter, encoding, batch_size, log):
    conn = sqlite3.connect(db_path)
    conn.execute('PRAGMA journal_mode=OFF')
    conn.execute('PRAGMA synchronous=OFF')
    conn.execute('PRAGMA temp_store=MEMORY')
    conn.execute('''
        CREATE TABLE csv_rows (
            id INTEGER PRIMARY KEY,
            match_key TEXT NOT NULL,
            md5 TEXT,
            size INTEGER,
            created TEXT,
            created_has_time INTEGER,
            modified TEXT,
            modified_has_time INTEGER,
            accessed TEXT,
            accessed_has_time INTEGER,
            row_num INTEGER,
            evidence TEXT,
            path TEXT,
            name TEXT,
            matched INTEGER DEFAULT 0
        )
    ''')

    batch = []
    row_num = 0
    with open(csv_path, 'r', newline='', encoding=encoding, errors='replace') as f:
        reader = csv.DictReader(f, delimiter=delimiter)
        required = [columns['name'], columns['path'], columns['evidence'], columns['md5']]
        missing = [c for c in required if c not in (reader.fieldnames or [])]
        if missing:
            raise SystemExit(
                "CSV is missing required column(s): {}\nColumns found in CSV: {}\n"
                "Use the --col-* arguments if your export uses different headers."
                .format(missing, reader.fieldnames))

        for row in reader:
            row_num += 1
            name = row.get(columns['name'], '') or ''
            path = row.get(columns['path'], '') or ''
            evidence = row.get(columns['evidence'], '') or ''
            md5 = normalize_md5(row.get(columns['md5'], ''))
            # Evidence is intentionally excluded from the match key - see
            # strip_leading_segment() for why the two exports' evidence
            # labels can't be compared directly. It's still stored/reported
            # on the row for reference (and used to group timestamps for
            # per-evidence time zone calibration).
            key = md5 + '::' + normalize_path_key(path, strip_extraction_annotation(name))
            size = parse_int(row.get(columns['size'])) if columns['size'] else None
            created, created_ht = parse_csv_timestamp_naive(row.get(columns['created'])) \
                if columns['created'] else (None, False)
            modified, modified_ht = parse_csv_timestamp_naive(row.get(columns['modified'])) \
                if columns['modified'] else (None, False)
            accessed, accessed_ht = parse_csv_timestamp_naive(row.get(columns['accessed'])) \
                if columns['accessed'] else (None, False)

            batch.append((
                key, md5, size,
                created.isoformat() if created else None, int(created_ht),
                modified.isoformat() if modified else None, int(modified_ht),
                accessed.isoformat() if accessed else None, int(accessed_ht),
                row_num, evidence, path, name,
            ))
            if len(batch) >= batch_size:
                _flush_csv_batch(conn, batch)
                batch.clear()
                if row_num % (batch_size * 20) == 0:
                    log(f"  ...indexed {row_num:,} CSV rows")

    if batch:
        _flush_csv_batch(conn, batch)

    log("  ...building lookup index (this can take a while on huge CSVs)")
    conn.execute('CREATE INDEX idx_csv_key ON csv_rows(match_key, matched)')
    conn.commit()
    log(f"CSV index built: {row_num:,} rows")
    return conn, row_num


def _flush_csv_batch(conn, batch):
    conn.executemany(
        'INSERT INTO csv_rows '
        '(match_key, md5, size, created, created_has_time, modified, modified_has_time, '
        'accessed, accessed_has_time, row_num, evidence, path, name) '
        'VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)',
        batch)
    conn.commit()


# ---------------------------------------------------------------------------
# Stage 2: derive each evidence object's true UTC offset from files that
# already identity-match (by MD5+path) between the CSV and JSON, then
# validate every MediaFiles entry using that per-evidence offset.
# ---------------------------------------------------------------------------

# A CSV timestamp is only trusted as a calibration sample if it lands within
# this many seconds of a whole-hour offset from the JSON's UTC timestamp -
# large enough to absorb the parser's 1s rounding, small enough to reject
# timestamps that just happen to be close by coincidence.
CALIBRATION_SLOP_SECONDS = 90
# Minimum identity-matched samples required before trusting a per-evidence
# offset instead of falling back to --csv-tz-offset-hours.
CALIBRATION_MIN_SAMPLES = 3


def calibrate_evidence_offsets(json_paths, conn, default_offset_hours, log):
    """Every CSV row still carries its raw local wall-clock time (see
    build_csv_index). For entries that already identity-match between the
    CSV and JSON (MD5+path), the JSON's UTC 'Created'/'Written' timestamp is
    ground truth for what that local time actually was - so the offset(s)
    needed to convert it to UTC can be read straight off the data instead of
    guessed.

    An evidence object can need more than one offset: a partition's display
    time zone typically follows civil time (e.g. UK GMT/BST), so files
    written in different seasons land exactly one hour apart even though
    they're the same evidence object. Rather than model daylight-saving
    rules, every whole-hour offset with meaningful support in the data is
    kept as a candidate, and each row is matched against whichever candidate
    fits - no assumption about which calendar dates are 'summer'."""
    from collections import Counter, defaultdict
    cur = conn.cursor()
    votes = defaultdict(Counter)

    for json_path in json_paths:
        with open(json_path, 'rb') as f:
            for media in IJSON.items(f, 'value.item.Media.item'):
                for mf in (media.get('MediaFiles') or []):
                    md5 = normalize_md5(mf.get('MD5')) or normalize_md5(media.get('MD5'))
                    file_name = mf.get('FileName') or ''
                    file_path = mf.get('FilePath') or ''
                    key = md5 + '::' + normalize_path_key(strip_leading_segment(file_path), file_name)
                    cur.execute(
                        'SELECT evidence, created, created_has_time, modified, modified_has_time '
                        'FROM csv_rows WHERE match_key=? LIMIT 1', (key,))
                    row = cur.fetchone()
                    if row is None:
                        continue
                    evidence, csv_created, created_ht, csv_modified, modified_ht = row

                    for json_field, csv_iso, has_time in (
                        ('Created', csv_created, created_ht),
                        ('Written', csv_modified, modified_ht),
                    ):
                        if not csv_iso or not has_time:
                            continue
                        json_dt = parse_iso_timestamp(mf.get(json_field))
                        if json_dt is None:
                            continue
                        csv_naive = datetime.fromisoformat(csv_iso)
                        json_naive_utc = json_dt.astimezone(timezone.utc).replace(tzinfo=None)
                        delta_seconds = (csv_naive - json_naive_utc).total_seconds()
                        rounded_hours = round(delta_seconds / 3600)
                        if abs(delta_seconds - rounded_hours * 3600) <= CALIBRATION_SLOP_SECONDS:
                            votes[evidence][rounded_hours] += 1

    offsets = {}
    log("Calibrating per-evidence UTC offset candidates from identity-matched timestamps ...")
    for evidence, counter in sorted(votes.items()):
        total = sum(counter.values())
        if total < CALIBRATION_MIN_SAMPLES:
            log(f"  '{evidence}': only {total} sample(s), using default offset "
                f"{default_offset_hours:+.0f}h")
            continue
        # Keep every offset with real support (>=1% of samples and at least
        # CALIBRATION_MIN_SAMPLES), not just the single most common one.
        candidates = [h for h, n in counter.most_common()
                      if n >= CALIBRATION_MIN_SAMPLES and n / total >= 0.01]
        offsets[evidence] = candidates
        breakdown = ', '.join(f'{h:+d}h={n}' for h, n in counter.most_common())
        log(f"  '{evidence}': candidates {[f'{h:+d}h' for h in candidates]} "
            f"from {total} samples ({breakdown})")
    log(f"Default offset for evidence with no/insufficient samples: {default_offset_hours:+.0f}h")
    return offsets


def process_entry(json_path, media, mf, cur, evidence_offsets, default_offset_hours,
                   tolerance_seconds, stats, report_writer):
    md5 = normalize_md5(mf.get('MD5')) or normalize_md5(media.get('MD5'))
    file_name = mf.get('FileName') or ''
    file_path = mf.get('FilePath') or ''
    full_path_display = file_path + file_name
    key = md5 + '::' + normalize_path_key(strip_leading_segment(file_path), file_name)

    cur.execute(
        'SELECT id, md5, size, evidence, '
        'created, created_has_time, modified, modified_has_time, accessed, accessed_has_time '
        'FROM csv_rows WHERE match_key=? AND matched=0 LIMIT 1', (key,))
    row = cur.fetchone()
    if row is None:
        stats['not_found'] += 1
        report_writer.writerow([json_path, md5, full_path_display, 'NOT_FOUND_IN_CSV', ''])
        return

    (row_id, csv_md5, csv_size, evidence,
     csv_created, created_ht, csv_modified, modified_ht, csv_accessed, accessed_ht) = row
    cur.execute('UPDATE csv_rows SET matched=1 WHERE id=?', (row_id,))
    # Multiple candidate offsets can apply to one evidence object (e.g. a
    # partition's export follows civil time, so summer/winter files differ
    # by an hour) - a row is accepted if ANY candidate reconciles it.
    candidate_offsets = evidence_offsets.get(evidence) or [default_offset_hours]

    problems = []
    if csv_md5 and md5 and csv_md5 != md5:
        problems.append(('md5_mismatch', f'MD5 mismatch (json={md5} csv={csv_md5})'))

    json_size = parse_int(media.get('MediaSize'))
    if json_size is not None and csv_size is not None and json_size != csv_size:
        problems.append(('size_mismatch', f'Size mismatch (json={json_size} csv={csv_size})'))

    for label, json_field, csv_iso, has_time in (
        ('Created', 'Created', csv_created, created_ht),
        ('Written', 'Written', csv_modified, modified_ht),
        ('Accessed', 'Accessed', csv_accessed, accessed_ht),
    ):
        jt = parse_iso_timestamp(mf.get(json_field))
        if not csv_iso:
            ct_display = None
            ok = jt is None
        elif has_time:
            csv_naive = datetime.fromisoformat(csv_iso)
            ok = False
            ct_display = localize_to_utc(csv_naive, candidate_offsets[0])
            for offset_hours in candidate_offsets:
                ct = localize_to_utc(csv_naive, offset_hours)
                if timestamps_match(jt, ct, tolerance_seconds):
                    ok = True
                    ct_display = ct
                    break
        else:
            # Date-only CSV value (e.g. some 'Accessed' exports carry no
            # time-of-day) - a fixed-offset UTC conversion would spuriously
            # push it across a day boundary, so compare calendar dates in
            # the evidence's own local zone instead of an exact instant.
            csv_naive = datetime.fromisoformat(csv_iso)
            ct_display = csv_naive.date()
            ok = jt is not None and any(
                jt.astimezone(timezone(timedelta(hours=offset_hours))).date() == csv_naive.date()
                for offset_hours in candidate_offsets)
        if not ok:
            problems.append(('timestamp_mismatch', f'{label} mismatch (json={jt} csv={ct_display})'))

    if problems:
        for kind, _ in problems:
            stats[kind] += 1
        report_writer.writerow([
            json_path, md5, full_path_display, 'FIELD_MISMATCH',
            '; '.join(msg for _, msg in problems)])
    else:
        stats['matched'] += 1


def validate(json_paths, conn, evidence_offsets, default_offset_hours, tolerance_seconds,
             report_writer, log):
    stats = {
        'json_entries': 0,
        'matched': 0,
        'not_found': 0,
        'md5_mismatch': 0,
        'size_mismatch': 0,
        'timestamp_mismatch': 0,
        'media_without_files': 0,
    }
    cur = conn.cursor()
    for json_path in json_paths:
        log(f"Validating {json_path} ...")
        with open(json_path, 'rb') as f:
            for media in IJSON.items(f, 'value.item.Media.item'):
                media_files = media.get('MediaFiles')
                if not media_files:
                    stats['media_without_files'] += 1
                    continue
                for mf in media_files:
                    stats['json_entries'] += 1
                    if stats['json_entries'] % 50000 == 0:
                        log(f"  ...checked {stats['json_entries']:,} file entries")
                    process_entry(json_path, media, mf, cur, evidence_offsets, default_offset_hours,
                                  tolerance_seconds, stats, report_writer)
        conn.commit()
    return stats


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def parse_args():
    p = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--csv', required=True, help='CSV file list exported from X-Ways')
    p.add_argument('--json', action='append', required=True, dest='json_paths',
                    help='VICS JSON file to validate (repeat for pictures + movies)')
    p.add_argument('--report', default='validation_report.csv',
                    help='Output CSV path for per-entry issues (default: validation_report.csv)')
    p.add_argument('--orphan-report', default=None,
                    help='Optional output CSV path listing CSV rows never referenced by any JSON entry')
    p.add_argument('--work-db', default=None,
                    help='Path for the scratch SQLite index (default: a temp file, deleted after the run)')
    p.add_argument('--keep-work-db', action='store_true',
                    help='Do not delete the scratch SQLite index after the run')
    p.add_argument('--csv-delimiter', default='\t',
                    help="Field delimiter in the CSV/TSV file (default: tab, X-Ways' export format)")
    p.add_argument('--csv-encoding', default='utf-16',
                    help="Encoding of the CSV file (default: utf-16, X-Ways' export format; "
                         "use 'utf-8-sig' for a comma-separated export)")
    p.add_argument('--csv-tz-offset-hours', type=float, default=0.0,
                    help='Fallback hours-to-UTC applied to an evidence object only when too few '
                         'identity-matched files exist to auto-calibrate its offset from the data '
                         '(default: 0). Per-evidence offsets are normally derived automatically - '
                         'see the calibration log lines.')
    p.add_argument('--timestamp-tolerance-seconds', type=float, default=2.0,
                    help='Allowed difference between JSON and CSV timestamps (default: 2s, for FILETIME/format rounding)')
    p.add_argument('--batch-size', type=int, default=5000,
                    help='Rows per SQLite insert batch while indexing the CSV (default: 5000)')

    g = p.add_argument_group('CSV column mapping (override if your export uses different headers)')
    g.add_argument('--col-name', default='Name')
    g.add_argument('--col-path', default='Path')
    g.add_argument('--col-evidence', default='Evidence object')
    g.add_argument('--col-md5', default='Hash¹')
    g.add_argument('--col-size', default='Size')
    g.add_argument('--col-created', default='Created')
    g.add_argument('--col-modified', default='Modified')
    g.add_argument('--col-accessed', default='Accessed')

    return p.parse_args()


def main():
    args = parse_args()
    columns = {
        'name': args.col_name, 'path': args.col_path, 'evidence': args.col_evidence,
        'md5': args.col_md5, 'size': args.col_size, 'created': args.col_created,
        'modified': args.col_modified, 'accessed': args.col_accessed,
    }

    def log(msg):
        print(f'[{time.strftime("%H:%M:%S")}] {msg}', flush=True)

    for jp in args.json_paths:
        if not os.path.isfile(jp):
            sys.exit(f"JSON file not found: {jp}")
    if not os.path.isfile(args.csv):
        sys.exit(f"CSV file not found: {args.csv}")

    if args.work_db:
        work_db_path = args.work_db
    else:
        fd, work_db_path = tempfile.mkstemp(suffix='.sqlite', prefix='vics_validate_')
        os.close(fd)  # sqlite3.connect() reopens the (empty) file itself
    log(f"Building CSV index at {work_db_path} ...")
    conn, csv_row_count = build_csv_index(
        args.csv, work_db_path, columns, args.csv_delimiter, args.csv_encoding,
        args.batch_size, log)

    evidence_offsets = calibrate_evidence_offsets(
        args.json_paths, conn, args.csv_tz_offset_hours, log)

    with open(args.report, 'w', newline='', encoding='utf-8') as report_f:
        report_writer = csv.writer(report_f)
        report_writer.writerow(['JSONFile', 'MD5', 'Path', 'IssueType', 'Details'])
        stats = validate(args.json_paths, conn, evidence_offsets, args.csv_tz_offset_hours,
                          args.timestamp_tolerance_seconds, report_writer, log)

    cur = conn.cursor()
    cur.execute('SELECT COUNT(*) FROM csv_rows WHERE matched=0')
    orphan_count = cur.fetchone()[0]

    if args.orphan_report:
        log(f"Writing orphan CSV rows to {args.orphan_report} ...")
        with open(args.orphan_report, 'w', newline='', encoding='utf-8') as orphan_f:
            writer = csv.writer(orphan_f)
            writer.writerow(['RowNum', 'Evidence', 'Path', 'Name', 'MD5'])
            cur.execute('SELECT row_num, evidence, path, name, md5 FROM csv_rows WHERE matched=0')
            while True:
                rows = cur.fetchmany(5000)
                if not rows:
                    break
                writer.writerows(rows)

    conn.close()
    if not args.keep_work_db:
        try:
            os.remove(work_db_path)
        except OSError:
            pass

    log("=== Summary ===")
    log(f"CSV rows indexed:                            {csv_row_count:,}")
    log(f"JSON MediaFiles entries checked:             {stats['json_entries']:,}")
    log(f"  Matched cleanly:                           {stats['matched']:,}")
    log(f"  Not found in CSV:                          {stats['not_found']:,}")
    log(f"  MD5 mismatches:                             {stats['md5_mismatch']:,}")
    log(f"  Size mismatches:                            {stats['size_mismatch']:,}")
    log(f"  Timestamp mismatches:                       {stats['timestamp_mismatch']:,}")
    log(f"  Media records with no MediaFiles entries:   {stats['media_without_files']:,}")
    log(f"CSV rows never referenced by any JSON entry:  {orphan_count:,}")
    log(f"Detailed report: {args.report}")
    if args.orphan_report:
        log(f"Orphan report:   {args.orphan_report}")

    if stats['not_found'] or stats['md5_mismatch'] or stats['size_mismatch'] or stats['timestamp_mismatch']:
        sys.exit(1)


if __name__ == '__main__':
    main()
