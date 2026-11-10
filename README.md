# xdg-desktop-portal-dde

A backend implementation for xdg-desktop-portal on Deepin desktop environment

## Interface Status

### Implemented

* org.freedesktop.impl.portal.Screenshot
* org.freedesktop.impl.portal.WallPaper
* org.freedesktop.impl.portal.Notification
* org.freedesktop.impl.portal.FileChooser
* org.freedesktop.impl.portal.GlobalShortcuts (v2; requires matching dde-services)

### TODO

* org.freedesktop.impl.portal.Screenshot
* org.freedesktop.impl.portal.WallPaper
* org.freedesktop.impl.portal.Notification
* org.freedesktop.impl.portal.FileChooser

### TBD

* org.freedesktop.impl.portal.Email
* org.freedesktop.impl.portal.DynamicLauncher

## Dependenices

### Build Dependenices

* Qt 6

### Installation

Build from source code 

1. Make sure you have installed all Dependenices

```bash
sudo apt build-dep ./
```

2. Build:

```bash
cmake -B build
cmake --build build -j$(nproc)
```

### Debug

Now you can use this project to Debug

[ashpd-demo](https://github.com/bilelmoussaoui/ashpd)

GlobalShortcuts v2 integration and regression checks are documented in
[Portal verification](tests/PORTAL_TESTING.md).
Deploy the matching `dde-services` plugin/provider (proposed release 1.0.51+) and
a GlobalShortcuts v2 broker (1.22+ or a complete backport).

### Getting help

### License

xdg-desktop-portal-dde is licensed under [LGPL-3.0-or-later](LICENSES)
