// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "screencastcontext.h"
#include "protocols/shmbuffer.h"
#include "loggings.h"
#include "pipewirecore.h"

#include <drm_fourcc.h>
#include <fcntl.h>
#include <sys/mman.h>

ScreenCastContext::ScreenCastContext(QObject *parent)
    : QObject(parent)
    , m_pwCore(new PipeWireCore(this))
    , m_shm(new WLShm)
    , m_linuxDmaBuf(new LinuxDmaBufV1)
    , m_outputImageCaptureSourceManager(new OutputImageCaptureSourceManager)
    , m_foreignToplevelImageCaptureSourceManager(new ForeignToplevelImageCaptureSourceManager)
    , m_imageCopyCaptureManager(new ImageCopyCaptureManager)
    , m_foreignToplevelList(new ForeignToplevelList)
    , m_shmInterfaceActive(false)
    , m_linuxDmaBufInterfaceActive(false)
    , m_outputImageCaptureSourceManagerActive(false)
    , m_foreignToplevelImageCaptureSourceManagerActive(false)
    , m_imageCopyCaptureManagerActive(false)
    , m_foreignToplevelListActive(false)
{
    m_linuxDmaBufInterfaceActive = m_linuxDmaBuf->isActive();
    connect(m_linuxDmaBuf, &LinuxDmaBufV1::activeChanged, this, [this]{
        m_linuxDmaBufInterfaceActive = m_linuxDmaBuf->isActive();
    });

    m_shmInterfaceActive = m_shm->isActive();
    connect(m_shm, &LinuxDmaBufV1::activeChanged, this, [this]{
        m_shmInterfaceActive = m_shm->isActive();
    });

    m_outputImageCaptureSourceManagerActive = m_outputImageCaptureSourceManager->isActive();
    connect(m_outputImageCaptureSourceManager, &OutputImageCaptureSourceManager::activeChanged, this, [this]{
        m_outputImageCaptureSourceManagerActive = m_outputImageCaptureSourceManager->isActive();
    });

    m_foreignToplevelImageCaptureSourceManagerActive = m_foreignToplevelImageCaptureSourceManager->isActive();
    connect(m_foreignToplevelImageCaptureSourceManager, &ForeignToplevelImageCaptureSourceManager::activeChanged, this, [this]{
        m_foreignToplevelImageCaptureSourceManagerActive = m_foreignToplevelImageCaptureSourceManager->isActive();
    });

    m_imageCopyCaptureManagerActive = m_imageCopyCaptureManager->isActive();
    connect(m_imageCopyCaptureManager, &ImageCopyCaptureManager::activeChanged, this, [this]{
        m_imageCopyCaptureManagerActive = m_imageCopyCaptureManager->isActive();
    });

    m_foreignToplevelListActive = m_foreignToplevelList->isActive();
    connect(m_foreignToplevelList, &ForeignToplevelList::activeChanged, this, [this]{
        m_foreignToplevelListActive = m_foreignToplevelList->isActive();
        connect(m_foreignToplevelList, &ForeignToplevelList::toplevelAdded,
                this, &ScreenCastContext::handleToplevelAdded);
        connect(m_foreignToplevelList, &ForeignToplevelList::finished,
                this, &ScreenCastContext::handleFinished);

    });
    m_state.timer_poll_fd = m_pwCore->m_notifier->socket();
    wl_list_init(&m_state.timers);
}

ScreenCastContext::~ScreenCastContext()
{
    struct xdpw_timer *timer, *ttmp;
    wl_list_for_each_safe(timer, ttmp, &m_state.timers, link) {
        if (timer->user_data == this) {
            xdpw_destroy_timer(timer);
        }
    }

    delete m_shm;

    m_linuxDmaBuf->destroy();
    m_linuxDmaBuf = nullptr;

    m_outputImageCaptureSourceManager->destroy();
    delete m_outputImageCaptureSourceManager;

    m_foreignToplevelImageCaptureSourceManager->destroy();
    delete m_foreignToplevelImageCaptureSourceManager;

    m_imageCopyCaptureManager->destroy();
    delete m_imageCopyCaptureManager;
}

wl_buffer *ScreenCastContext::createWLSHMBuffer(int fd, wl_shm_format fmt, int width, int height, int stride)
{
    if (!m_shm) {
        qCCritical(SCREENCAST) << "error, WLShm is nullptr";
        return nullptr;
    }

    if (!m_shmInterfaceActive) {
        qCCritical(SCREENCAST) << "error, WLShm is deactive";
        return nullptr;
    }

    int size = stride * height;

    if (fd < 0) {
        qCCritical(SCREENCAST) << "error, fd < 0";
        return nullptr;
    }

    struct wl_shm_pool *pool = m_shm->create_pool(fd, size);
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, fmt);
    wl_shm_pool_destroy(pool);

    return buffer;
}

gbm_device *ScreenCastContext::createGBMDeviceFromDRMDevice(drmDevice *device)
{
    if (!(device->available_nodes & (1 << DRM_NODE_RENDER))) {
        qCCritical(SCREENCAST, "DRM device has no render node");
        return nullptr;
    }

    const char *render_node = device->nodes[DRM_NODE_RENDER];
    qCWarning(SCREENCAST, "xdpw: Using render node %s", render_node);

    int fd = open(render_node, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        qCCritical(SCREENCAST, "xdpw: Could not open render node %s", render_node);

        return nullptr;
    }

    return gbm_create_device(fd);
}

bool ScreenCastContext::linuxDmaBufInterfaceActive() const
{
    return m_linuxDmaBufInterfaceActive;
}

bool ScreenCastContext::shmInterfaceActive() const
{
    return m_shmInterfaceActive;
}

bool ScreenCastContext::outputImageCaptureSourceManagerActive() const
{
    return m_outputImageCaptureSourceManagerActive;
}

bool ScreenCastContext::foreignToplevelImageCaptureSourceManagerActive() const
{
    return m_foreignToplevelImageCaptureSourceManagerActive;
}

bool ScreenCastContext::imageCopyCaptureManagerActive() const
{
    return m_imageCopyCaptureManagerActive;
}

QList<ToplevelInfo *> ScreenCastContext::toplevels() const
{
    return m_toplevels;
}

void ScreenCastContext::handleToplevelAdded(ForeignToplevelHandle *toplevel)
{
    auto info = new ToplevelInfo(toplevel);
    connect(info->handle, &ForeignToplevelHandle::closed,
            this, &ScreenCastContext::handleToplevelClosed);
    connect(info->handle, &ForeignToplevelHandle::appIdChanged,
            this, &ScreenCastContext::handleToplevelAppIdChanged);
    connect(info->handle, &ForeignToplevelHandle::identifierChanged,
            this, &ScreenCastContext::handleIdentifierChanged);
    m_toplevels << info;
}

void ScreenCastContext::handleFinished()
{
    foreach (ToplevelInfo *info, m_toplevels) {
        delete info;
    }
    m_toplevels.clear();
}

void ScreenCastContext::handleToplevelClosed()
{
    auto handle = static_cast<ForeignToplevelHandle *>(sender());
    for (int i = 0; i < m_toplevels.count(); i++) {
        auto toplevel = m_toplevels[i];
        if (toplevel->handle == handle) {
            m_toplevels.removeOne(toplevel);
            delete toplevel;
            return;
        }
    }
}

void ScreenCastContext::handleToplevelAppIdChanged(const QString &appId)
{
    auto handle = static_cast<ForeignToplevelHandle *>(sender());
    foreach (ToplevelInfo *info, m_toplevels) {
        if (info->handle == handle) {
            info->appID = appId;
            return;
        }
    }
}

void ScreenCastContext::handleIdentifierChanged(const QString &identifier)
{
    auto handle = static_cast<ForeignToplevelHandle *>(sender());
    foreach (ToplevelInfo *info, m_toplevels) {
        if (info->handle == handle) {
            info->identifier = identifier;
            return;
        }
    }
}
