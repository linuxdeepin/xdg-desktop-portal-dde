// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "protocols/linuxdmabufer.h"
#include "protocols/screencopy.h"
#include "protocols/imagecapturesource.h"
#include "protocols/imagecopycapture.h"
#include "toplevelmodel.h"

#include <QObject>
#include <QElapsedTimer>

#include <gbm.h>
#include <xf86drm.h>
#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>

#include <QList>

#include <memory>

#define TIMESPEC_NSEC_PER_SEC 1000000000L

class WLShm;
class PipeWireCore;
class PipeWireStream;
class AbstractPipeWireStream;
class OutputPipeWireStream;
class ToplevelPipeWireStream;

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
    bool linuxDmaBufInterfaceActive() const;
    bool shmInterfaceActive() const;
    bool outputImageCaptureSourceManagerActive() const;
    bool foreignToplevelImageCaptureSourceManagerActive() const;
    bool imageCopyCaptureManagerActive() const;
    bool foreignToplevelListActive() const;
    bool pipeWireAvailable();
    QList<ToplevelInfoPtr> toplevels() const;
    bool containsToplevel(const QString &identifier) const;
    static gbm_device *createGBMDeviceFromDRMDevice(drmDevice *device);

private Q_SLOTS:
    void handleToplevelAdded(ForeignToplevelHandle *toplevel);
    void handleFinished();
    void handleToplevelClosed();
    void handleToplevelAppIdChanged(const QString &appId);
    void handleIdentifierChanged(const QString &identifier);

private:
    void initializeToplevelList();
    std::shared_ptr<PipeWireCore> pipeWireCore();

    friend class PipeWireStream;
    friend class AbstractPipeWireStream;
    friend class OutputPipeWireStream;
    friend class ToplevelPipeWireStream;

    std::shared_ptr<PipeWireCore> m_pwCore;
    QElapsedTimer m_pipeWireRetryTimer;
    WLShm *m_shm;
    LinuxDmaBufV1 *m_linuxDmaBuf;
    OutputImageCaptureSourceManager *m_outputImageCaptureSourceManager;
    ForeignToplevelImageCaptureSourceManager *m_foreignToplevelImageCaptureSourceManager;
    ImageCopyCaptureManager *m_imageCopyCaptureManager;
    ForeignToplevelListPtr m_foreignToplevelList;
    QList<ToplevelInfoPtr> m_toplevels;
    bool m_linuxDmaBufInterfaceActive;
    bool m_shmInterfaceActive;
    bool m_outputImageCaptureSourceManagerActive;
    bool m_foreignToplevelImageCaptureSourceManagerActive;
    bool m_imageCopyCaptureManagerActive;
    bool m_foreignToplevelListActive;
    bool m_toplevelListInitialized = false;
};
