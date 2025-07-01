// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "qwayland-wayland.h"

#include <QObject>
#include <QWaylandClientExtension>

class WLShmPool
    : public QWaylandClientExtensionTemplate<WLShmPool>
    , public QtWayland::wl_shm_pool
{
    Q_OBJECT
public:
    WLShmPool();
    ~WLShmPool() override;
};

class WLShm
    : public QWaylandClientExtensionTemplate<WLShm>
    , public QtWayland::wl_shm
{
    Q_OBJECT
public:
    WLShm();
    ~WLShm() override;

protected:
    void shm_format(uint32_t format) override;

Q_SIGNALS:
    void formatChanged(uint32_t format);
};

class WLBuffer
    : public QWaylandClientExtensionTemplate<WLBuffer>
    , public QtWayland::wl_buffer
{
    Q_OBJECT
public:
    WLBuffer();
    ~WLBuffer() override;

protected:
    void buffer_release() override;

Q_SIGNALS:
    void releaseed();
};




