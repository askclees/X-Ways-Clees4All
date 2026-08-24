# Clees4All (C4All)

Clees4All is an [X-Ways Forensics](https://www.x-ways.net/forensics/) X-Tension (a native DLL plugin) that exports picture and video evidence from an X-Ways case into formats suitable for import into third-party review tools, most notably [Griffeye Analyze](https://www.griffeye.com/).

It runs as part of the Refine Volume Snapshot (RVS) process and, for every picture/video item it finds, can write:

- a **[Project VICS](https://kindredtech.org)** JSON export (`VICS_Pictures_Results.json` / `VICS_Movies_Results.json`), optionally packaged as a compressed zip archive alongside the source files, and/or
- a **C4All XML** export (`... C4P Index.xml` / `... C4M Index.xml`), the project's own legacy format.

It can also drive Griffeye Analyze's command-line case creation so a fully populated case is waiting once export finishes.

Full step-by-step usage instructions, including screenshots of every dialog, are in **[docs/USER_GUIDE.md](docs/USER_GUIDE.md)**.

## Features

- Exports pictures and/or videos independently, with per-type min/max file size filters
- Filters by X-Ways **type status** (confirmed, mismatch detected, etc.) and **file format consistency**
- Excludes embedded `Thumbnail.jpg` files, with an option to still include ones flagged as a thumbnail mismatch
- Excludes media carved from within already-exported live video files, avoiding duplicate export
- Deduplicates files by MD5 + physical offset, preferring live over deleted over carved copies
- Optional VICS JSON output, C4All XML output, or both, with VICS output optionally written straight into a zip archive
- Optional automatic Griffeye Analyze case creation, including a named Griffeye import-settings profile
- Remembers your last-used settings between runs (stored in a local SQLite database)
- Scriptable via X-Ways `XTParam:` command-line arguments for unattended/batch processing
- Multi-threaded; tested with X-Ways RVS running up to 8 threads

## Requirements

- X-Ways Forensics 19.2 or later (some features require newer versions — see [docs/USER_GUIDE.md](docs/USER_GUIDE.md) for the version matrix)
- Windows 7 or later, 64-bit
- The custom C4All file-type signature definitions, imported into X-Ways for the file header signature search step — bundled with the [releases](../../releases) rather than kept in this repo
- Griffeye Analyze with command-line case creation support, only if automatic Griffeye case creation is used

## Getting started

1. Import the C4All file-type signatures (from the [release](../../releases) download) into X-Ways Forensics (File Header Signatures editor).
2. Add `XT_Clees4All_<version>.dll` to X-Ways as an X-Tension (Options > Run X-Tensions).
3. Run RVS on your evidence with the file header signature search, hashing, and file type verification enabled, then run RVS again with the Clees4All X-Tension ticked.

See [docs/USER_GUIDE.md](docs/USER_GUIDE.md) for the full walkthrough, recommended RVS settings, dialog reference, and command-line automation reference.

## Building from source

The project builds as a 64-bit Windows DLL using the Code::Blocks project file `X-Ways C4All.cbp` with the `gcc-mingw32` toolchain (C++17, statically linked runtime and third-party libraries: libarchive, zlib, cJSON, SQLite).

```
# From a MinGW-w64 environment with Code::Blocks / codeblocks-cli:
codeblocks --build "X-Ways C4All.cbp" --target=Release
```

The built DLL is written to `bin/Release/`.

### Tests and tooling

- `tests/` contains a small unit-test harness (base64, file output, VICS JSON) — see `tests/testharness.h`.
- `tools/validate_vics_json.py` cross-checks a completed VICS JSON export against an X-Ways file-list export (hash, size, and timestamp consistency). Requires `pip install ijson`.

## License

GNU General Public License v3.0 — see [LICENSE](LICENSE).

# VICS

The VICS Data Model is a standard format maintained by Kindred Tech (https://kindredtech.org) for exchanging digital evidence and related information between forensic and investigative tools. Within the X-Ways plugin, VICS allows media and associated information, such as file hashes, paths, timestamps, categorisation and PhotoDNA values, to be exported in a consistent format that can be used by other VICS-compatible systems.
