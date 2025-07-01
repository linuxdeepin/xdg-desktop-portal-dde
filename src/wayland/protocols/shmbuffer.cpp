// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "shmbuffer.h"

#define WLSHMPOOLVERSION 2
#define WLSHMVERSION 2
#define WLBUFFERVERSION 1

WLShmPool::WLShmPool()
    : QWaylandClientExtensionTemplate<WLShmPool>(WLSHMPOOLVERSION)
{
}

WLShmPool::~WLShmPool()
{
}

WLShm::WLShm()
    : QWaylandClientExtensionTemplate<WLShm>(WLSHMVERSION)
{
}

WLShm::~WLShm()
{
}

void WLShm::shm_format(uint32_t format)
{
    Q_EMIT formatChanged(format);
}

WLBuffer::WLBuffer()
    : QWaylandClientExtensionTemplate<WLBuffer>(WLBUFFERVERSION)
{
}

WLBuffer::~WLBuffer()
{
}

void WLBuffer::buffer_release()
{
    Q_EMIT releaseed();
}
