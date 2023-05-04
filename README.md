# xdg-desktop-portal-dde

A backend implement for xdg-desktop-portal on Deepin desktop environment

## Interface have been implemented

* org.freedesktop.impl.portal.Screenshot
* org.freedesktop.impl.portal.WallPaper
* org.freedesktop.impl.portal.Notification
* org.freedesktop.impl.portal.FileChooser

## On the plan

* org.freedesktop.impl.portal.Screenshot
* org.freedesktop.impl.portal.WallPaper
* org.freedesktop.impl.portal.Notification
* org.freedesktop.impl.portal.FileChooser

## Maybe

* org.freedesktop.impl.portal.Email
* org.freedesktop.impl.portal.DynamicLauncher

## Dependenices

### Build Dependenices

* Qt >= 5.12

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

### Getting help

### License

xdg-desktop-portal-dde is licensed under [LGPL-3.0-or-later](LICENSES)

