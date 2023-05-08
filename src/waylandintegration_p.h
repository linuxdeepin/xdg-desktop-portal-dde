// Copyright © 2018 Red Hat, Inc
// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef XDG_DESKTOP_PORTAL_KDE_WAYLAND_INTEGRATION_P_H
#define XDG_DESKTOP_PORTAL_KDE_WAYLAND_INTEGRATION_P_H

#include "waylandintegration.h"

#include <QDateTime>
#include <QObject>
#include <QMap>

#include <gbm.h>

#include <epoxy/egl.h>
#include <epoxy/gl.h>

class ScreenCastStream;

namespace KWayland {
    namespace Client {
        class ConnectionThread;
        class EventQueue;
        class OutputDevice;
        class Registry;
        class RemoteAccessManager;
        class RemoteBuffer;
        class Output;
    }
}

namespace WaylandIntegration
{

class WaylandIntegrationPrivate : public WaylandIntegration::WaylandIntegration
{
    Q_OBJECT
public:
    typedef struct {
        uint nodeId;
        QVariantMap map;
    } Stream;
    typedef QList<Stream> Streams;

    WaylandIntegrationPrivate();
    ~WaylandIntegrationPrivate();

    void initDrm();
    void initEGL();
    void initWayland();

    bool isEGLInitialized() const;

    void bindOutput(int outputName, int outputVersion);
    bool startStreaming(const WaylandOutput &output);
    void stopStreaming();
    QMap<quint32, WaylandOutput> screens();
    QVariant streams();

protected Q_SLOTS:
    void addOutput(quint32 name, quint32 version);
    void removeOutput(quint32 name);
    void processBuffer(const KWayland::Client::RemoteBuffer *rbuf);
    void setupRegistry();

private:
    bool m_eglInitialized;
    bool m_streamingEnabled;
    bool m_registryInitialized;

    quint32 m_output;
    QDateTime m_lastFrameTime;
    ScreenCastStream *m_stream;

    QThread *m_thread;

    QMap<quint32, WaylandOutput> m_outputMap;
    QList<KWayland::Client::Output*> m_bindOutputs;

    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::EventQueue *m_queue;
    KWayland::Client::Registry *m_registry;
    KWayland::Client::RemoteAccessManager *m_remoteAccessManager;

    qint32 m_drmFd = 0; // for GBM buffer mmap
    gbm_device *m_gbmDevice = nullptr; // for passed GBM buffer retrieval
    struct {
        QList<QByteArray> extensions;
        EGLDisplay display = EGL_NO_DISPLAY;
        EGLContext context = EGL_NO_CONTEXT;
    } m_egl;
};

}

#endif // XDG_DESKTOP_PORTAL_KDE_WAYLAND_INTEGRATION_P_H


