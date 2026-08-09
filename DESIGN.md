# CachyOS Software Catalog Design

## Product Direction

The application is a fast catalog for CachyOS users, not a replacement package manager. It preserves the reference application's three source families and mutation model while presenting one coherent catalog: curated applications, native repositories, and Flatpak apps/runtimes. The visual language is Fluent 2-inspired rather than a platform-emulating clone: a quiet neutral canvas, blue accent, layered cards, generous spacing, compact status pills, and a navigation rail that collapses naturally on narrow windows.

## Reference-Compatible Behavior

1. Startup runs as a normal user, enforces one instance, rejects an invalid/empty pacman database, and refuses to start while `/var/lib/pacman/db.lck` exists.
2. Curated entries come from the authoritative remote `pkglist.yaml` with an installed local fallback. An entry may contain multiple space-separated native package targets; the first target supplies its primary metadata and the complete target list is used for the action.
3. Native repository data comes from configured libalpm sync databases. Installed and upgradable states are calculated against the local database. Refresh rereads local metadata; it does not synchronize databases.
4. Before a native install/remove, libalpm prepares a transaction with `DB_ONLY`, dependency/explicit-target flags, and `NO_LOCK`. The preview shows affected packages, dependency additions, removals, conflicts, and download/installed/removed size summaries. The user must confirm before `pkexec pacman` runs.
5. Actual native actions are `pkexec pacman -S`, `pkexec pacman -R`, and `pkexec pacman -Syu`; orphan removal remains a separately warned action. No custom daemon or privileged resident process is introduced.
6. Flatpak lists remote apps and runtimes, hides Locale/Sources/Debug extensions, detects installed refs by selected scope, supports system and user scopes, filters, updates, unused-runtime cleanup, remote management, and Flatpakref URL/path installation. Every query and mutation receives the selected scope.
7. A live console shows stdout and stderr, supports interactive stdin where pacman/Flatpak requests it, exposes cancellation, and retains failure output for diagnosis. Logs rotate in the Qt cache location.

## Runtime Architecture

```text
QML shell
  -> AppController (queued QObject API, settings/theme, selection, operation state)
      -> CatalogModel / PackageModel / FlatpakModel (roles for cards, details, actions)
      -> RepositoryWorker (dedicated thread: pacman-conf + libalpm + AppStream)
      -> ProcessRunner (async QProcess, argv-safe, streamed output, pkexec)
      -> FlatpakService (argv construction, parsing, scope and remote policy)
      -> CuratedCatalog (remote/local YAML parsing)
      -> LogService (rotating diagnostic log)
```

The worker owns libalpm handles and is never accessed from QML or the GUI thread. It returns value objects only. Each repository reload rebuilds a temporary database snapshot, so installed/update states cannot remain stale after a mutation. `ProcessRunner` serializes mutating operations so a second transaction cannot overlap the first, and it checks the lock again immediately before starting a privileged pacman process. Preview and mutation are intentionally separate to mirror pacman's real lock acquisition.

## UI Model

- Navigation rail: Home, Native repositories, Flatpak, and Settings.
- Home: greeting/status strip, search, category chips, curated application cards, selected-action bar, and a detail pane/dialog.
- Native: searchable virtualized list, status filter (all/installed/upgradable/not installed), library/development toggle, package count summary, refresh, upgrade-all, orphan action, and package details.
- Flatpak: remote and scope selectors, app/runtime filters, searchable list, install/update/remove actions, manage-remotes and Flatpakref entry points.
- Operation sheet: preview summary before confirmation; progress and cancellable console drawer while running; explicit success/failure state after completion.
- Details use AppStream name, summary, description, icon, screenshots, homepage/license when present, and package version/status. Missing metadata falls back to pacman package data.

## Themes And Persistence

System, Light, and Dark are persistent settings. QML colors are centralized in `Theme.qml`; System follows `QStyleHints::colorScheme` and updates when the desktop scheme changes. Window geometry, theme, last page, Flatpak scope/remote, library filter, and existing reference keys such as `disableWarning`/`showFlatpak` are stored with `QSettings`.

## Metadata And Performance

AppStream's local pool is loaded once in the repository worker. It is indexed by package name and AppStream ID; icon stock names are resolved by Qt's theme, local filenames are loaded only on demand, and remote artwork is not downloaded during startup. Lists are value-backed models with incremental filtering and QML `ListView` virtualization. Curated network retrieval has a finite timeout and local fallback; a stale cache remains usable offline.

## Failure And Safety States

Missing pacman/Flatpak/AppStream tools, offline catalogue, empty sync databases, lock contention, authentication cancellation, invalid Flatpakref, broken remote, dependency conflict, cancellation, and process failure are visible states with actionable output. Tests use fake process runners and libalpm fixtures for these paths; host verification is read-only.

## Translation And Packaging

All user-visible C++/QML strings use Qt translation APIs. Qt Linguist catalogs are compiled into resources, with new shell coverage in German, Spanish, and Turkish plus the reference locale catalog set retained for Azerbaijani, Belarusian, Bulgarian, Catalan, Czech, Hebrew, Italian, Japanese, Georgian, Korean, Polish, Russian, Slovak, Swedish, and Ukrainian. The desktop entry, icon, Polkit policy, local curated fallback, help/license assets, and translations are installed with the binary using relocatable CMake paths; runtime code does not assume `/usr`.
