# DESIGN.md

Redesigned CachyOS Package Installer — architecture, decisions and
deliberate deviations from the reference implementation.

## Overview

A modern Qt 6 desktop software catalog that reproduces the complete
functionality and safety model of the reference CachyOS packageinstaller:
curated ("Popular") applications, full repository browsing, Flatpak
discovery/management, transaction previews, polkit/pkexec operations,
console output, settings and translations — with a Fluent 2-inspired UI and
a responsive, non-blocking data layer.

## Layer architecture

```
src/ui/            Qt Widgets UI (nav rail + pages) — main thread only
src/models/        QAbstractTableModel + proxy filters (24k-row tables)
src/core/          alpm_manager, pacman_cache, flatpak, pkglist, cmd,
                   appstream_provider, pacman_conf, versionnumber, logging
src/main.cpp       startup guards, translations, theme bootstrap, test hook
```

Data flow: worker threads (QtConcurrent) produce immutable snapshots
(package rows, flatpak refs, AppStream pool); they are handed to the UI via
queued `QMetaObject::invokeMethod` calls guarded with `QPointer` so the UI
never blocks on enumeration, network or Flatpak listing. Privileged
operations run through `Cmd` (async QProcess streaming to the console page).

## Preserved reference behavior (parity)

- ALPM transaction previews are always `DB_ONLY | ALL_DEPS | ALL_EXPLICIT |
  NO_LOCK`, never committed; conflict/unsatisfied-dep messages follow the
  reference wording (`'a' and 'b' are in conflict (reason)`, broken-dep
  warnings).
- `prepareAddTrans` reports conflicts through the message string; the UI
  shows the Replace / Ignore dialog (Replace proceeds; installs keep running
  `pkexec pacman -S` without `--noconfirm`, removals always use
  `--noconfirm` — reference `is_ok` semantics).
- All privileged pacman operations go through `pkexec`; all Flatpak
  operations use `flatpak` CLI with `socat` PTY wrappers so interactive
  prompts reach the console page (incl. `--user`/`--system` scopes,
  flatpakref with `\:` escaping, per-app uninstall chaining, `--unused`
  removal, first-visit `flatpak update --appstream`, flathub/verified remote
  bootstrap when flatpak gets installed on demand).
- Startup guards: single instance (`CachyOS-PI-lock` QSharedMemory), valid
  ALPM DBs, root refusal (`logname` + uid), `/var/lib/pacman/db.lck`,
  log rotation to `cachyospi.log.old`.
- Curated list comes from `pkglist.yaml` (network refresh from the CachyOS
  repo, cached under the config dir, bundled qrc fallback) with the same
  category/subgroup nesting as the reference.
- Flatpak status filters, remote/scope combos, duplicate short-name
  disambiguation, Lib/Dev package filtering
  (`lib*` minus `libre*`, `-dev/-dbg/-dbgsym/-debug/-devel`) are preserved.
- Settings keys reused for parity: `theme`, `showFlatpak`, `disableWarning`,
  `geometry`.

## Deliberate improvements (documented deviations)

1. **Responsiveness.** The reference blocks the UI thread with nested event
   loops (`Cmd::getCmdOut`, package enumeration, Flatpak listing). This app
   moves all enumeration/listing/metadata work to worker threads; the UI
   renders immediately and updates when snapshots arrive.
2. **Scalable tables.** The reference builds one `QTreeWidgetItem` per
   package (~24k). This app uses `QAbstractTableModel` +
   `QSortFilterProxyModel`, so search/filter/sort stay interactive at full
   repo size.
3. **Multi-target previews.** The reference's `add_targets_to_install`
   added only the first resolvable target to the preview transaction
   (a `break`). Here every target (package, repo-qualified or group)
   is resolved, so dependency/conflict previews cover the real selection.
4. **Installed-state detection for Flatpak.** The reference compared
   remote-ls refs against `flatpak list` output formats that do not match on
   modern flatpak. Refs are normalized (prefix/arch stripped) and matched
   against the installed set, fixing installed/not-installed marking.
5. **AppStream metadata + screenshots.** libappstream pools (OS catalog,
   OS metainfo, Flatpak catalog) provide authentic names, summaries,
   developer/homepage data, theme icons and screenshot URLs for Discover
   cards and the detail panel (async loading with a disk cache).
6. **No Rust bridge.** The reference's alpm backend is a CXX-bridged Rust
   crate. The same semantics are implemented directly on the libalpm C API
   (`alpm_add_pkg`, `alpm_trans_prepare`, `alpm_pkg_vercmp`, …), removing a
   heavy toolchain dependency while keeping every observable behavior.
7. **Version comparison** uses `alpm_pkg_vercmp` (as before) and installed
   state comes from the ALPM local DB instead of `pacman -Q` subprocesses.
8. **`Architecture = auto` resolution** adds the CPU microarchitecture level
   (`x86_64_v2/v3/v4`) exactly like CachyOS's pacman, which the reference's
   pacmanconf parser leaves to the Rust crate defaults — required for the
   CachyOS v3 repositories.
9. **Theme engine.** Palette + generated QSS with a documented token map
   (see `theme.cpp`); "System" resolves against the platform palette
   captured once, so switching away and back behaves predictably.

## UI design (Fluent 2-inspired)

- Left navigation rail (icon + label, selection pill, Settings pinned to the
  bottom). Top-level pages: Discover, Packages, Flatpak, Console (opened by
  operations), Settings.
- **Discover**: category card grid → package card grid; each card carries an
  AppStream icon, summary, version and status badge with an action button
  (Install / Update / Remove) and batch selection; clicking a card opens the
  detail panel (screenshot, description, metadata, action).
- **Packages**: full repo table with search, status filter
  (All/Upgradable/Installed/Not installed), lib/dev hiding, upgrade-all and
  orphan removal; double-click shows full `pacman -Si`-equivalent details
  read straight from ALPM.
- **Flatpak**: scope (system/user) + remote combo, filter set identical to
  the reference, counts, manage-remotes dialog (add/remove remote,
  flatpakref install), update-all and remove-unused.
- **Console**: streaming output with an input line for interactive prompts.
- **Settings**: theme (System/Light/Dark, persisted), Flatpak page toggle,
  warning preference, about.
- Dark palette targets the Fluent dark tokens (surfaces #2B2B2B on #1F1F1F,
  accent #4CC2FF); light uses #FFFFFF/#F3F3F3 with #0067C0 accent.

## Safety model

- The app refuses to run as root; privileged work goes exclusively through
  polkit (`pkexec`), so every system mutation requires authentication.
- Transactions are previewed (dependency resolution + conflict detection)
  before any `pkexec pacman` invocation; the confirmation dialog is defaulted
  to Cancel.
- Flatpak operations are always confirmed with the visible target list.
- A single running instance is enforced; starting while `pacman` holds the
  database lock is refused.

## Testing

- `tests/run_tests.sh` runs the full scripted suite offscreen with
  `tests/fakebin` (fake pkexec/pacman/flatpak/socat/logname + fixtures):
  install, cancel, uninstall, upgrade-all, orphan removal, failure path,
  Flatpak listing/install/uninstall/update/unused, transaction previews,
  conflict detection (real ALPM: `blas-openblas` vs installed `blas`),
  themes/screenshots, HiDPI-capable resizing, and the single-instance guard.
- The same harness runs unmodified on the real display (Wayland/X11) for
  visual verification.
- Verified manually against the host: full repo enumeration (~16k unique
  packages), search/filters, transaction previews and Flatpak listing via
  the real system `flatpak`.

## Known limitations

- Screenshots in the detail panel are fetched over the network (AppStream
  URLs); offline they degrade to a placeholder.
- The Fakebin suite cannot exercise the real `pkexec` polkit prompt or
  genuine network downloads; those paths rely on the reference-parity
  command construction (asserted in the suite).
- Flatpak scope switching performs the flathub remote bootstrap exactly like
  the reference; in restricted network environments this may be slow.
