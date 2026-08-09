# AGENTS.md

Agent guide for working on the redesigned CachyOS Package Installer
(`cachyos-pi`) in this directory.

## Ground truth

- The authoritative reference implementation lives in
  `~/Workspace/cachyos pkg manager/` (the official CachyOS packageinstaller
  repo). **It is read-only — never modify it.**
- This app preserves the reference's package-management behavior, safety model
  and architecture. When in doubt about behavior, consult the reference
  sources (`src/mainwindow.cpp`, `backend-rustlib/src/lib.rs`, `src/*.cpp`)
  before changing anything here.

## Build & run

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/cachyos-pi                # run
```

Dependencies: Qt 6 (Widgets, Network, Concurrent, LinguistTools), libalpm
(>= 13), libappstream (>= 1.0), polkit + pkexec, flatpak, socat, pacman.

## Tests

```sh
./tests/run_tests.sh build/cachyos-pi
```

Runs the scripted end-to-end harness (env `CACHYOS_PI_UI_TEST=<report-dir>`)
against `tests/fakebin` (fake pkexec/pacman/flatpak/socat/logname) plus the
single-instance guard check. Nothing touches the host system. Reports go to
`build/test-report/report.txt` + screenshots. All flows must PASS before
committing changes.

The harness is compiled into the binary (`src/ui_test_harness.cpp`, gated by
the env var); `MainWindow` exposes small test hooks (`*Public` methods,
accessors) for it.

## Architecture map

| Area | Files | Notes |
|---|---|---|
| ALPM backend | `src/alpm_manager.{h,cpp}`, `src/pacman_conf.{h,cpp}`, `src/pacman_cache.{h,cpp}` | libalpm C API; DB_ONLY preview transactions, conflict/dep reporting identical to the reference Rust backend |
| Process runner | `src/cmd.{h,cpp}` | async QProcess for pkexec/flatpak ops; `runShellCommand` for worker-thread blocking calls |
| Flatpak | `src/flatpak.{h,cpp}` | CLI integration; user/system scopes; socat PTY wrappers (reference parity) |
| Curated list | `src/pkglist.{h,cpp}` | pkglist.yaml fetch + purpose-built YAML parser; bundled qrc fallback |
| Metadata | `src/appstream_provider.{h,cpp}` | libappstream AsPool (OS + Flatpak catalogs) loaded in a worker; icons/summaries/screenshots |
| Models | `src/models/package_model.*`, `flatpak_model.*` | QTableView model/proxy (24k+ rows) |
| UI | `src/ui/` | nav rail, Discover cards/detail, repo table, flatpak table, console, settings |
| Themes | `src/theme.{h,cpp}` | System/Light/Dark, palette + generated QSS with a documented token map |
| Translations | `lang/*.ts` → build/lang/*.qm → embedded qrc | regenerate with `lupdate6 src -ts lang/<app>_<lang>.ts` |

## Critical invariants

- **Never run package transactions through libalpm.** Previews are
  `DB_ONLY|ALL_DEPS|ALL_EXPLICIT|NO_LOCK` and always released without commit.
  Real operations run `pkexec pacman …` / `flatpak …` (reference safety model).
- Startup guards (single instance via `CachyOS-PI-lock`, valid DBs, root
  refusal, `/var/lib/pacman/db.lck`) must never be weakened.
- `Architecture = auto` must resolve to the machine arch + CPU microarch
  levels (x86_64_v2/v3/v4) — CachyOS ships v3 packages; forgetting this
  breaks every transaction preview.
- libalpm 16 API notes: `alpm_add_pkg`/`alpm_remove_pkg` (not
  `trans_add_pkg`), `alpm_pkg_download_size`, `alpm_option_add_architecture`
  copies strings (the `set_architectures` variant takes ownership), and
  `ALPM_ERR_PKG_INVALID_ARCH` prepare data is a list of formatted
  `name-ver-arch` **strings**, not package pointers.
- Keep the theme QSS token map (`src/theme.cpp` stylesheet comment) in sync
  with the `.arg()` list — a mismatch silently paints surfaces with the text
  color.
- ALPM/flatpak/AppStream handles are not thread-safe: alpm calls are
  serialized by an internal mutex, Flatpak/ApmStream work runs in worker
  threads and returns to the UI thread via queued `QMetaObject::invokeMethod`
  with `QPointer` guards.
