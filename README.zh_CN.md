# xdg-desktop-portal-dde

xdg-desktop-portal 在 Deepin desktop 环境的后端实现

## 已经实现(预留的)接口

* org.freedesktop.impl.portal.Screenshot
* org.freedesktop.impl.portal.WallPaper
* org.freedesktop.impl.portal.Notification
* org.freedesktop.impl.portal.FileChooser

## 目标实现的接口

* org.freedesktop.impl.portal.Screenshot
* org.freedesktop.impl.portal.WallPaper
* org.freedesktop.impl.portal.Notification
* org.freedesktop.impl.portal.FileChooser

## 以后可能会实现的接口

* org.freedesktop.impl.portal.Email
* org.freedesktop.impl.portal.DynamicLauncher

## 依赖

### 构建依赖

* Qt >= 5.12

### Installation

源码构建

1. 确保你安装了所有依赖

```bash
sudo apt build-dep ./
```

2. 构建:

```bash
cmake -B build
cmake --build build -j$(nproc)
```

### Debug

可以使用以下项目进行debug

[ashpd-demo](https://github.com/bilelmoussaoui/ashpd)

### Getting help

### License

xdg-desktop-portal-dde 遵循协议 [LGPL-3.0-or-later](LICENSES)

