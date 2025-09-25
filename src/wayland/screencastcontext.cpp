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

#include <QSocketNotifier>

static void on_core_error(void *data, uint32_t id, int seq, int res, const char* message) {
    qCFatal(SCREENCAST, "pipewire: fatal error event from core");
}

static const struct pw_core_events core_events = {
    .version = PW_VERSION_CORE_EVENTS,
    .error = on_core_error,
};

static struct spa_hook core_listener;

ScreenCastContext::ScreenCastContext(QObject *parent)
    : QObject(parent)
    , m_pwCore(new PipeWireCore(this))
    , m_screenCopyManager(new ScreenCopyManager)
    , m_shm(new WLShm)
    , m_linuxDmaBuf(new LinuxDmaBufV1)
    , m_linuxDmaBufFeedback(nullptr)
    , m_gbmDevice(nullptr)
    , m_forceModLinear(false)
    , m_shmInterfaceActive(false)
    , m_linuxDmaBufInterfaceActive(false)
{
    m_screenCopyManagerActive = m_screenCopyManager->isActive();
    connect(m_screenCopyManager, &ScreenCopyManager::activeChanged, this, [this]{
        m_screenCopyManagerActive = m_screenCopyManager->isActive();
    });

    m_linuxDmaBufInterfaceActive = m_linuxDmaBuf->isActive();
    if (m_linuxDmaBufInterfaceActive) {
        initConnection();
    }

    connect(m_linuxDmaBuf, &LinuxDmaBufV1::activeChanged, this, [this]{
        m_linuxDmaBufInterfaceActive = m_linuxDmaBuf->isActive();
        if (m_linuxDmaBufInterfaceActive) {
            initConnection();
        }

    });

    m_shmInterfaceActive = m_shm->isActive();
    connect(m_shm, &LinuxDmaBufV1::activeChanged, this, [this]{
        m_shmInterfaceActive = m_shm->isActive();
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

    m_screenCopyManager->destroy();
    delete m_screenCopyManager;

    delete m_shm;

    if (m_linuxDmaBufFeedback) {
        m_linuxDmaBufFeedback->destroy();
        m_linuxDmaBufFeedback = nullptr;
    }

    m_linuxDmaBuf->destroy();
    m_linuxDmaBuf = nullptr;

    gbm_device_destroy(m_gbmDevice);
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


bool ScreenCastContext::queryDMABufModifiers(uint32_t drmFormat,
                                             uint32_t numModifiers,
                                             uint64_t *modifiers,
                                             uint32_t *maxModifiers)
{
    if (m_formatModifierPairs.isEmpty()) {
        return false;
    }

    if (numModifiers == 0) {
        *maxModifiers = 0;
        foreach(auto fm_pair , m_formatModifierPairs) {
                if (fm_pair.fourcc == drmFormat &&
                    (fm_pair.modifier == DRM_FORMAT_MOD_INVALID ||
                     gbm_device_get_format_modifier_plane_count(m_gbmDevice, fm_pair.fourcc, fm_pair.modifier) > 0))
                    *maxModifiers += 1;
        }
        return true;
    }

    uint32_t i = 0;
    foreach(auto fm_pair , m_formatModifierPairs) {
        if (i == numModifiers)
            break;
        if (fm_pair.fourcc == drmFormat &&
            (fm_pair.modifier == DRM_FORMAT_MOD_INVALID ||
             gbm_device_get_format_modifier_plane_count(m_gbmDevice, fm_pair.fourcc, fm_pair.modifier) > 0)) {
            modifiers[i] = fm_pair.modifier;
            i++;
        }
    }
    *maxModifiers = numModifiers;
    return true;
}

bool ScreenCastContext::linuxDmaBufInterfaceActive() const
{
    return m_linuxDmaBufInterfaceActive;
}

bool ScreenCastContext::shmInterfaceActive() const
{
    return m_shmInterfaceActive;
}

bool ScreenCastContext::screenCopyManagerActive() const
{
    return m_screenCopyManagerActive;
}

void ScreenCastContext::handleLinuxDmaBufModifierChanged(uint32_t format,
                                                         uint32_t modifierHigh,
                                                         uint32_t modifierLow)
{
    uint64_t modifier = (((uint64_t)modifierHigh) << 32) | modifierLow;
    qCDebug(SCREENCAST) << "handleLinuxDmaBufModifierChanged" << format << modifier;
    addFormatModifierPair(format, modifier);
}

void ScreenCastContext::handlevDmaBufFeedbackDone()
{
    if (m_feedbackData.formatTableData) {
        munmap(m_feedbackData.formatTableData, m_feedbackData.formatTableSize);
    }
    m_feedbackData.formatTableData = nullptr;
    m_feedbackData.formatTableSize = 0;
}

void ScreenCastContext::handlevDmaBufFeedbackFormatTableReceived(int fd, uint size)
{
    m_formatModifierPairs.clear();

    m_feedbackData.formatTableData = mmap(nullptr , size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (m_feedbackData.formatTableData == MAP_FAILED) {
        qCCritical(SCREENCAST) << "mmap failed in handlevDmaBufFeedbackFormatTableReceived";
        m_feedbackData.formatTableData = nullptr;
        m_feedbackData.formatTableSize = 0;
        return;
    }
    m_feedbackData.formatTableSize = size;
}

void ScreenCastContext::handlevDmaBufFeedbackMainDeviceReceived(const wl_array *deviceArr)
{
    dev_t device;
    assert(deviceArr->size == sizeof(device));
    memcpy(&device, deviceArr->data, sizeof(device));

    drmDevice *drmDev = nullptr;
    if (drmGetDeviceFromDevId(device, 0, &drmDev) != 0) {
        qCCritical(SCREENCAST) << "unable to open main device" << drmDev->nodes;
        m_forceModLinear = true;
        return;
    }
    m_gbmDevice = createGBMDeviceFromDRMDevice(drmDev);
    drmFreeDevice(&drmDev);
}

void ScreenCastContext::handlevDmaBufFeedbackTrancheDone()
{
    m_feedbackData.deviceUsed = false;
}

void ScreenCastContext::handlevDmaBufFeedbackTrancheTargetDeviceReceived(const wl_array *deviceArr)
{
    dev_t device;
    assert(deviceArr->size == sizeof(device));
    memcpy(&device, deviceArr->data, sizeof(device));

    drmDevice *drmDev = nullptr;
    if (drmGetDeviceFromDevId(device, 0, &drmDev) != 0) {
        return;
    }

    if (m_gbmDevice) {
        drmDevice *drmDevRenderer = nullptr;
        drmGetDevice2(gbm_device_get_fd(m_gbmDevice), 0, &drmDevRenderer);
        m_feedbackData.deviceUsed = drmDevicesEqual(drmDevRenderer, drmDev);
    } else {
        m_gbmDevice = createGBMDeviceFromDRMDevice(drmDev);
        m_feedbackData.deviceUsed = m_gbmDevice;
    }
    drmFreeDevice(&drmDev);
}

void ScreenCastContext::handlevDmaBufFeedbackTrancheFormatsReceived(const wl_array *indices)
{
    if (!m_feedbackData.deviceUsed || !m_feedbackData.formatTableData) {
        return;
    }
    struct fm_entry {
        uint32_t format;
        uint32_t padding;
        uint64_t modifier;
    };
    static_assert(sizeof(fm_entry) == 16, "fm_entry size must be 16 bytes");

    uint32_t n_modifiers = m_feedbackData.formatTableSize / sizeof(fm_entry);
    fm_entry *fm_entries = static_cast<fm_entry *>(m_feedbackData.formatTableData);

    const uint16_t *idx = static_cast<const uint16_t *>(indices->data);
    size_t count = indices->size / sizeof(uint16_t);
    for (size_t i = 0; i < count; ++i) {
        if (idx[i] >= n_modifiers) {
            continue;
        }
        addFormatModifierPair(fm_entries[idx[i]].format, fm_entries[idx[i]].modifier);
    }
}

void ScreenCastContext::initConnection()
{
    qCDebug(SCREENCAST) << "linux dmabuf version" << m_linuxDmaBuf->version();
    if (m_linuxDmaBuf->version() >= 4) {
        m_linuxDmaBufFeedback = new LinuxDmaBufFeedbackV1(m_linuxDmaBuf->get_default_feedback(), m_linuxDmaBuf);
        connect(m_linuxDmaBufFeedback, &LinuxDmaBufFeedbackV1::feedbackDone,
                this, &ScreenCastContext::handlevDmaBufFeedbackDone);
        connect(m_linuxDmaBufFeedback, &LinuxDmaBufFeedbackV1::formatTableReceived,
                this, &ScreenCastContext::handlevDmaBufFeedbackFormatTableReceived);
        connect(m_linuxDmaBufFeedback, &LinuxDmaBufFeedbackV1::mainDeviceReceived,
                this, &ScreenCastContext::handlevDmaBufFeedbackMainDeviceReceived);
        connect(m_linuxDmaBufFeedback, &LinuxDmaBufFeedbackV1::trancheDone,
                this, &ScreenCastContext::handlevDmaBufFeedbackTrancheDone);
        connect(m_linuxDmaBufFeedback, &LinuxDmaBufFeedbackV1::trancheTargetDeviceReceived,
                this, &ScreenCastContext::handlevDmaBufFeedbackTrancheTargetDeviceReceived);
        connect(m_linuxDmaBufFeedback, &LinuxDmaBufFeedbackV1::trancheFormatsReceived,
                this, &ScreenCastContext::handlevDmaBufFeedbackTrancheFormatsReceived);
    } else {
        connect(m_linuxDmaBuf, &LinuxDmaBufV1::modifierChanged,
                this, &ScreenCastContext::handleLinuxDmaBufModifierChanged);
    }
}

void ScreenCastContext::addFormatModifierPair(uint32_t format, uint64_t modifier)
{
    foreach (auto modifierPair, m_formatModifierPairs) {
        if (modifierPair.fourcc == format && modifierPair.modifier == modifier) {
            qCWarning(SCREENCAST, "skipping duplicated format %u (%lu)", modifierPair.fourcc, modifierPair.modifier);
            return;
        }
    }

    m_formatModifierPairs.append(DRMFormatModifierPair{format, modifier});
}
