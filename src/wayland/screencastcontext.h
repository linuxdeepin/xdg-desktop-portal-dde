// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "protocols/linuxdmabufer.h"
#include "protocols/screencopy.h"
#include "pipewiretimer.h"

#include <QObject>

#include <gbm.h>
#include <xf86drm.h>
#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>

#include <QList>

#define TIMESPEC_NSEC_PER_SEC 1000000000L

class WLShm;
class PipeWireCore;
class PipeWireStream;

struct DMABufFeedbackData {
    void *formatTableData;
    uint32_t formatTableSize;
    bool deviceUsed;
};

struct DRMFormatModifierPair {
    uint32_t fourcc;
    uint64_t modifier;
};

class ScreenCastContext : public QObject
{
    Q_OBJECT
public:
    ScreenCastContext(QObject *parent = nullptr);
    ~ScreenCastContext() override;

    wl_buffer *createWLSHMBuffer(int fd,
                                 enum wl_shm_format fmt,
                                 int width,
                                 int height,
                                 int stride);
    bool queryDMABufModifiers(uint32_t drmFormat,
                                    uint32_t numModifiers,
                                    uint64_t *modifiers,
                                    uint32_t *maxModifiers);
    bool linuxDmaBufInterfaceActive() const;
    bool shmInterfaceActive() const;
    bool screenCopyManagerActive() const;
private Q_SLOTS:
    void handleLinuxDmaBufModifierChanged(uint32_t format,
                                          uint32_t modifierHigh,
                                          uint32_t modifierLow);
    void handlevDmaBufFeedbackDone();
    void handlevDmaBufFeedbackFormatTableReceived(int fd, uint size);
    void handlevDmaBufFeedbackMainDeviceReceived(const wl_array *device);
    void handlevDmaBufFeedbackTrancheDone();
    void handlevDmaBufFeedbackTrancheTargetDeviceReceived(const wl_array *device);
    void handlevDmaBufFeedbackTrancheFormatsReceived(const wl_array *indices);

private:
    void initConnection();
    void addFormatModifierPair(uint32_t format, uint64_t modifier);
    gbm_device *createGBMDeviceFromDRMDevice(drmDevice *device);

private:
    friend class PipeWireStream;

    PipeWireCore *m_pwCore;
    ScreenCopyManager *m_screenCopyManager;
    WLShm *m_shm;
    LinuxDmaBufV1 *m_linuxDmaBuf;
    LinuxDmaBufFeedbackV1 *m_linuxDmaBufFeedback;
    gbm_device *m_gbmDevice;
    DMABufFeedbackData m_feedbackData;
    QList<DRMFormatModifierPair> m_formatModifierPairs;
    bool m_forceModLinear;
    bool m_linuxDmaBufInterfaceActive;
    bool m_shmInterfaceActive;
    bool m_screenCopyManagerActive;
    struct xdpw_state m_state;
};
