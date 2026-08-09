# Development Guide

## Scope

`app/` is the clean-room Qt rewrite. `../cachyos pkg manager/` is a read-only reference and must not be edited, built in place, or used as an output directory.

## Architecture Rules

- Keep libalpm read-only: metadata, installed state, upgrade detection, dependency/conflict previews, and size summaries only.
- Keep system mutations in pacman and Flatpak CLI processes launched with `QProcess` argv lists. Never concatenate user, package, remote, URL, or path input into a shell command.
- Keep long-running work outside the GUI thread and stream process output into the console surface.
- Preserve normal-user startup, single-instance, pacman lock, database validity, Polkit, system/user Flatpak scope, and cancellation semantics.
- Prefer Qt and the standard library over new dependencies. Keep QML presentation separate from service/model code.

## Build And Test

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build --output-on-failure
QT_QPA_PLATFORM=offscreen CACHYOS_CATALOG_OFFLINE=1 ./build/cachyos-catalog
```

Run normal UI checks with `./build/cachyos-catalog`. Do not run install, remove, upgrade, remote-add, remote-delete, or Flatpakref operations against the host during tests.
