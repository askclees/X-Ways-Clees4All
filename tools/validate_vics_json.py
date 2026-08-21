#!/usr/bin/env python3
"""
Validates a Clees4All VICS JSON export (VICS_Pictures_Results.json /
VICS_Movies_Results.json) against a CSV file list exported from X-Ways,
checking that every MediaFiles entry in the JSON has a matching row in the
CSV with a consistent MD5 hash, file size, and MAC timestamps.

Built for multi-gigabyte inputs:
  - The CSV is streamed row-by-row into a disposable on-disk SQLite index
    (never held fully in memory), keyed on a normalised MD5+path string.
  - The JSON is parsed incrementally with ijson, so only one Media record
    (plus its MediaFiles) is ever materialised in memory at a time.

Requires: pip install ijson

Example:
    python validate_vics_json.py ^
        --csv xways_export.csv ^
        --json VICS_Pictures_Results.json --json VICS_Movies_Results.json ^
        --report mismatches.csv --orphan-report csv_only.csv

See the column-mapping arguments (--col-*) if your X-Ways export uses
different column headers than the defaults below.
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


def parse_csv_timestamp(value, tz_offset_hours):
    if not value:
        return None
    v = value.strip()
    if not v:
        return None
    for fmt in CSV_TIMESTAMP_FORMATS:
        try:
            dt = datetime.strptime(v, fmt)
            dt = dt.replace(tzinfo=timezone(timedelta(hours=tz_offset_hours)))
            return dt.astimezone(timezone.utc)
        except ValueError:
            continue
    return None


def timestamps_match(json_dt, csv_dt, tolerance_seconds):
    if json_dt is None and csv_dt is None:
        return True
    if json_dt is None or csv_dt is None:
        return False
    return abs((json_dt - csv_dt).total_seconds()) <= tolerance_seconds


# ---------------------------------------------------------------------------
# Stage 1: stream the CSV into a scratch SQLite index
# ---------------------------------------------------------------------------

def build_csv_index(csv_path, db_path, columns, delimiter, encoding, batch_size,
                     tz_offset_hours, log):
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
            modified TEXT,
            accessed TEXT,
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
            key = md5 + '::' + normalize_path_key(evidence, path, name)
            size = parse_int(row.get(columns['size'])) if columns['size'] else None
            created = parse_csv_timestamp(row.get(columns['created']), tz_offset_hours) \
                if columns['created'] else None
            modified = parse_csv_timestamp(row.get(columns['modified']), tz_offset_hours) \
                if columns['modified'] else None
            accessed = parse_csv_timestamp(row.get(columns['accessed']), tz_offset_hours) \
                if columns['accessed'] else None

            batch.append((
                key, md5, size,
                created.isoformat() if created else None,
                modified.isoformat() if modified else None,
                accessed.isoformat() if accessed else None,
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
        '(match_key, md5, size, created, modified, accessed, row_num, evidence, path, name) '
        'VALUES (?,?,?,?,?,?,?,?,?,?)',
        batch)
    conn.commit()


# ---------------------------------------------------------------------------
# Stage 2: stream the JSON and validate each MediaFiles entry against the index
# ---------------------------------------------------------------------------

def process_entry(json_path, media, mf, cur, tolerance_seconds, stats, report_writer):
    md5 = normalize_md5(mf.get('MD5')) or normalize_md5(media.get('MD5'))
    file_name = mf.get('FileName') or ''
    file_path = mf.get('FilePath') or ''
    full_path_display = file_path + file_name
    key = md5 + '::' + normalize_path_key(file_path, file_name)

    cur.execute(
        'SELECT id, md5, size, created, modified, accessed '
        'FROM csv_rows WHERE match_key=? AND matched=0 LIMIT 1', (key,))
    row = cur.fetchone()
    if row is None:
        stats['not_found'] += 1
        report_writer.writerow([json_path, md5, full_path_display, 'NOT_FOUND_IN_CSV', ''])
        return

    row_id, csv_md5, csv_size, csv_created, csv_modified, csv_accessed = row
    cur.execute('UPDATE csv_rows SET matched=1 WHERE id=?', (row_id,))

    problems = []
    if csv_md5 and md5 and csv_md5 != md5:
        problems.append(('md5_mismatch', f'MD5 mismatch (json={md5} csv={csv_md5})'))

    json_size = parse_int(media.get('MediaSize'))
    if json_size is not None and csv_size is not None and json_size != csv_size:
        problems.append(('size_mismatch', f'Size mismatch (json={json_size} csv={csv_size})'))

    for label, json_field, csv_iso in (
        ('Created', 'Created', csv_created),
        ('Written', 'Written', csv_modified),
        ('Accessed', 'Accessed', csv_accessed),
    ):
        jt = parse_iso_timestamp(mf.get(json_field))
        ct = datetime.fromisoformat(csv_iso) if csv_iso else None
        if not timestamps_match(jt, ct, tolerance_seconds):
            problems.append(('timestamp_mismatch', f'{label} mismatch (json={jt} csv={ct})'))

    if problems:
        for kind, _ in problems:
            stats[kind] += 1
        report_writer.writerow([
            json_path, md5, full_path_display, 'FIELD_MISMATCH',
            '; '.join(msg for _, msg in problems)])
    else:
        stats['matched'] += 1


def validate(json_paths, conn, tolerance_seconds, report_writer, log):
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
                    process_entry(json_path, media, mf, cur, tolerance_seconds, stats, report_writer)
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
    p.add_argument('--csv-delimiter', default=',')
    p.add_argument('--csv-encoding', default='utf-8-sig',
                    help="Encoding of the CSV file (default: utf-8-sig; use 'utf-16' if X-Ways exported UTF-16)")
    p.add_argument('--csv-tz-offset-hours', type=float, default=0.0,
                    help='Hours to add to CSV timestamps to convert them to UTC - match whatever '
                         'time zone your X-Ways case is configured to display (default: 0 = already UTC)')
    p.add_argument('--timestamp-tolerance-seconds', type=float, default=2.0,
                    help='Allowed difference between JSON and CSV timestamps (default: 2s, for FILETIME/format rounding)')
    p.add_argument('--batch-size', type=int, default=5000,
                    help='Rows per SQLite insert batch while indexing the CSV (default: 5000)')

    g = p.add_argument_group('CSV column mapping (override if your export uses different headers)')
    g.add_argument('--col-name', default='Name')
    g.add_argument('--col-path', default='Path')
    g.add_argument('--col-evidence', default='Evidence Object')
    g.add_argument('--col-md5', default='Hash Value')
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
        args.batch_size, args.csv_tz_offset_hours, log)

    with open(args.report, 'w', newline='', encoding='utf-8') as report_f:
        report_writer = csv.writer(report_f)
        report_writer.writerow(['JSONFile', 'MD5', 'Path', 'IssueType', 'Details'])
        stats = validate(args.json_paths, conn, args.timestamp_tolerance_seconds, report_writer, log)

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
