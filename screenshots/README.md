# Verification Screenshots

- `home-hidpi.png` is the light/system shell rendered offscreen at `QT_SCALE_FACTOR=2`.
- `home-dark-hidpi.png` is the dark theme rendered at the same scale.
- `home-compact.png` is a narrow `980x640` layout check.

Generate them without host mutations with:

```sh
QT_QPA_PLATFORM=offscreen CACHYOS_CATALOG_OFFLINE=1 ./build/cachyos-catalog --screenshot screenshots/home-hidpi.png
```
