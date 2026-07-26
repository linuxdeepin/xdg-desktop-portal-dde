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

#include <climits>
#include <cstdint>
#include <algorithm>
#include <utility>

ScreenCastContext::ScreenCastContext(QObject *parent)
    : QObject(parent)
    , m_shm(new WLShm)
    , m_linuxDmaBuf(new LinuxDmaBufV1)
    , m_outputImageCaptureSourceManager(new OutputImageCaptureSourceManager)
    , m_foreignToplevelImageCaptureSourceManager(new ForeignToplevelImageCaptureSourceManager)
    , m_imageCopyCaptureManager(new ImageCopyCaptureManager)
    , m_foreignToplevelList(createForeignToplevelList())
    , m_shmInterfaceActive(false)
    , m_linuxDmaBufInterfaceActive(false)
    , m_outputImageCaptureSourceManagerActive(false)
    , m_foreignToplevelImageCaptureSourceManagerActive(false)
    , m_imageCopyCaptureManagerActive(false)
    , m_foreignToplevelListActive(false)
{
    m_pipeWireRetryTimer.start();
    m_pwCore = std::make_shared<PipeWireCore>();

    m_linuxDmaBufInterfaceActive = m_linuxDmaBuf->isActive();
    connect(m_linuxDmaBuf, &LinuxDmaBufV1::activeChanged, this, [this]{
        m_linuxDmaBufInterfaceActive = m_linuxDmaBuf->isActive();
    });

    m_shmInterfaceActive = m_shm->isActive();
    connect(m_shm, &WLShm::activeChanged, this, [this]{
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
    initializeToplevelList();
    connect(m_foreignToplevelList.data(), &ForeignToplevelList::activeChanged, this, [this]{
        m_foreignToplevelListActive = m_foreignToplevelList->isActive();
        initializeToplevelList();
    });
}

ScreenCastContext::~ScreenCastContext()
{
    m_toplevels.clear();
    m_foreignToplevelList.clear();

    delete m_shm;
    m_shm = nullptr;

    m_linuxDmaBuf->destroy();
    delete m_linuxDmaBuf;
    m_linuxDmaBuf = nullptr;

    m_outputImageCaptureSourceManager->destroy();
    delete m_outputImageCaptureSourceManager;

    m_foreignToplevelImageCaptureSourceManager->destroy();
    delete m_foreignToplevelImageCaptureSourceManager;

    m_imageCopyCaptureManager->destroy();
    delete m_imageCopyCaptureManager;
    m_imageCopyCaptureManager = nullptr;
}

void ScreenCastContext::initializeToplevelList()
{
    if (!m_foreignToplevelListActive || m_toplevelListInitialized) {
        return;
    }

    m_toplevelListInitialized = true;
    connect(m_foreignToplevelList.data(), &ForeignToplevelList::toplevelAdded,
            this, &ScreenCastContext::handleToplevelAdded);
    connect(m_foreignToplevelList.data(), &ForeignToplevelList::finished,
            this, &ScreenCastContext::handleFinished);
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

    if (fd < 0) {
        qCCritical(SCREENCAST) << "error, fd < 0";
        return nullptr;
    }

    if (width <= 0 || height <= 0 || stride <= 0) {
        qCCritical(SCREENCAST) << "error, invalid WLShm buffer geometry"
                              << width << height << stride;
        return nullptr;
    }

    const uint64_t size = static_cast<uint64_t>(stride)
            * static_cast<uint64_t>(height);
    if (size > static_cast<uint64_t>(INT_MAX)) {
        qCCritical(SCREENCAST) << "error, WLShm buffer is too large";
        return nullptr;
    }

    struct wl_shm_pool *pool =
            m_shm->create_pool(fd, static_cast<int>(size));
    if (!pool) {
        qCCritical(SCREENCAST) << "error, failed to create WLShm pool";
        return nullptr;
    }

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

bool ScreenCastContext::foreignToplevelListActive() const
{
    return m_foreignToplevelListActive;
}

std::shared_ptr<PipeWireCore> ScreenCastContext::pipeWireCore()
{
    constexpr qint64 retryIntervalMilliseconds = 1000;
    if (!m_pwCore
        || (!m_pwCore->isValid()
            && m_pipeWireRetryTimer.elapsed() >= retryIntervalMilliseconds)) {
        // Streams retain the core on which they were created. Replacing the
        // context's reference is therefore safe even while a failed stream is
        // still unwinding, and lets a later portal request recover after
        // PipeWire has restarted.
        m_pipeWireRetryTimer.restart();
        m_pwCore = std::make_shared<PipeWireCore>();
    }
    return m_pwCore;
}

bool ScreenCastContext::pipeWireAvailable()
{
    const std::shared_ptr<PipeWireCore> core = pipeWireCore();
    return core && core->isValid();
}

QList<ToplevelInfoPtr> ScreenCastContext::toplevels() const
{
    return m_toplevels;
}

bool ScreenCastContext::containsToplevel(const QString &identifier) const
{
    if (identifier.isEmpty()) {
        return false;
    }

    return std::any_of(m_toplevels.cbegin(),
                       m_toplevels.cend(),
                       [&identifier](const ToplevelInfoPtr &toplevel) {
        return toplevel && toplevel->identifier == identifier;
    });
}

void ScreenCastContext::handleToplevelAdded(ForeignToplevelHandle *toplevel)
{
    const ToplevelInfoPtr info =
            ToplevelInfoPtr::create(toplevel, m_foreignToplevelList);
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
    m_toplevels.clear();
}

void ScreenCastContext::handleToplevelClosed()
{
    auto handle = static_cast<ForeignToplevelHandle *>(sender());
    for (int i = 0; i < m_toplevels.count(); i++) {
        auto toplevel = m_toplevels[i];
        if (toplevel->handle == handle) {
            m_toplevels.removeOne(toplevel);
            return;
        }
    }
}

void ScreenCastContext::handleToplevelAppIdChanged(const QString &appId)
{
    auto handle = static_cast<ForeignToplevelHandle *>(sender());
    for (const ToplevelInfoPtr &info : std::as_const(m_toplevels)) {
        if (info->handle == handle) {
            info->appID = appId;
            return;
        }
    }
}

void ScreenCastContext::handleIdentifierChanged(const QString &identifier)
{
    auto handle = static_cast<ForeignToplevelHandle *>(sender());
    for (const ToplevelInfoPtr &info : std::as_const(m_toplevels)) {
        if (info->handle == handle) {
            info->identifier = identifier;
            return;
        }
    }
}
