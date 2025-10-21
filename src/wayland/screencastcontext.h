// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "protocols/linuxdmabufer.h"
#include "protocols/screencopy.h"
#include "pipewiretimer.h"
#include "protocols/imagecapturesource.h"
#include "protocols/imagecopycapture.h"
#include "toplevelmodel.h"

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
class AbstractPipeWireStream;
class OutputPipeWireStream;
class ToplevelPipeWireStream;

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
    bool outputImageCaptureSourceManagerActive() const;
    bool foreignToplevelImageCaptureSourceManagerActive() const;
    bool imageCopyCaptureManagerActive() const;
    QList<ToplevelInfo*> toplevels() const;
    static gbm_device *createGBMDeviceFromDRMDevice(drmDevice *device);
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

    void handleToplevelAdded(ForeignToplevelHandle *toplevel);
    void handleFinished();
    void handleToplevelClosed();
    void handleToplevelAppIdChanged(const QString &appId);
    void handleIdentifierChanged(const QString &identifier);
private:
    void initConnection();
    void addFormatModifierPair(uint32_t format, uint64_t modifier);

private:
    friend class PipeWireStream;
    friend class AbstractPipeWireStream;
    friend class OutputPipeWireStream;
    friend class ToplevelPipeWireStream;

    PipeWireCore *m_pwCore;
    ScreenCopyManager *m_screenCopyManager;
    WLShm *m_shm;
    LinuxDmaBufV1 *m_linuxDmaBuf;
    LinuxDmaBufFeedbackV1 *m_linuxDmaBufFeedback;
    OutputImageCaptureSourceManager *m_outputImageCaptureSourceManager;
    ForeignToplevelImageCaptureSourceManager *m_foreignToplevelImageCaptureSourceManager;
    ImageCopyCaptureManager *m_imageCopyCaptureManager;
    gbm_device *m_gbmDevice;
    ForeignToplevelList *m_foreignToplevelList;
    DMABufFeedbackData m_feedbackData;
    QList<DRMFormatModifierPair> m_formatModifierPairs;
    QList<ToplevelInfo*> m_toplevels;
    bool m_forceModLinear;
    bool m_linuxDmaBufInterfaceActive;
    bool m_shmInterfaceActive;
    bool m_screenCopyManagerActive;
    bool m_outputImageCaptureSourceManagerActive;
    bool m_foreignToplevelImageCaptureSourceManagerActive;
    bool m_imageCopyCaptureManagerActive;
    bool m_foreignToplevelListActive;
    struct xdpw_state m_state;
};
