// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "mpvvideoitem.h"
#include "loggings.h"

#include <QQuickWindow>
#include <QOpenGLFramebufferObject>
#include <QOpenGLContext>

static void *getGLProcAddress(void *ctx, const char *name)
{
    Q_UNUSED(ctx)

    QOpenGLContext *glctx = QOpenGLContext::currentContext();
    if (!glctx) {
        return nullptr;
    }

    return reinterpret_cast<void *>(glctx->getProcAddress(QByteArray(name)));
}

static void handleMpvRedraw(void *ctx)
{
    QMetaObject::invokeMethod(static_cast<MpvVideoItem *>(ctx), &MpvVideoItem::update, Qt::QueuedConnection);
}

MpvRenderer::MpvRenderer(MpvVideoItem *item)
    : m_item(item)
{
    m_item->window()->setPersistentSceneGraph(true);
}

QOpenGLFramebufferObject *MpvRenderer::createFramebufferObject(const QSize &size)
{
    if (!m_item->m_mpvGL) {
#if MPV_CLIENT_API_VERSION < MPV_MAKE_VERSION(2, 0)
        mpv_opengl_init_params gl_init_params{getGLProcAddress, nullptr, nullptr};
#else
        mpv_opengl_init_params gl_init_params{getGLProcAddress, nullptr};
#endif
        mpv_render_param display{MPV_RENDER_PARAM_INVALID, nullptr};
        if (QGuiApplication::platformName() == QStringLiteral("xcb") || QGuiApplication::platformName() == QStringLiteral("dxcb")) {
            display.type = MPV_RENDER_PARAM_X11_DISPLAY;
            display.data = qGuiApp->nativeInterface<QNativeInterface::QX11Application>()->display();
        }

        if (QGuiApplication::platformName() == QStringLiteral("wayland")) {
            display.type = MPV_RENDER_PARAM_WL_DISPLAY;
            display.data = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>()->display();
        }
        mpv_render_param params[]{{MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
                                   {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init_params},
                                   display,
                                   {MPV_RENDER_PARAM_INVALID, nullptr}};

        int result = mpv_render_context_create(&m_item->m_mpvGL, m_item->m_mpv, params);
        if (result < 0) {
            qCCritical(WALLPAPER) << "failed to initialize mpv GL context";
        } else {
            mpv_render_context_set_update_callback(m_item->m_mpvGL, handleMpvRedraw, m_item);
            Q_EMIT m_item->ready();
        }
    }

    return QQuickFramebufferObject::Renderer::createFramebufferObject(size);
}

void MpvRenderer::render()
{
    QOpenGLFramebufferObject *fbo = framebufferObject();
    mpv_opengl_fbo mpfbo;
    mpfbo.fbo = static_cast<int>(fbo->handle());
    mpfbo.w = fbo->width();
    mpfbo.h = fbo->height();
    mpfbo.internal_format = 0;

    int flip_y{0};

    mpv_render_param params[] = {{MPV_RENDER_PARAM_OPENGL_FBO, &mpfbo},
                                  {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
                                  {MPV_RENDER_PARAM_INVALID, nullptr}};
    mpv_render_context_render(m_item->m_mpvGL, params);
}

MpvVideoItem::MpvVideoItem(QQuickItem *parent)
    : QQuickFramebufferObject(parent)
{
    if (QQuickWindow::graphicsApi() != QSGRendererInterface::OpenGL) {
        qCCritical(WALLPAPER) << "error, The graphics api must be set to opengl or mpv won't be able to render the video.";
    }

    m_workerThread = new QThread;
    m_mpvController = new MpvVideoController;
    m_workerThread->start();
    m_mpvController->moveToThread(m_workerThread);
    QMetaObject::invokeMethod(m_mpvController, &MpvVideoController::init, Qt::BlockingQueuedConnection);

    m_mpv = m_mpvController->mpv();

    connect(m_workerThread, &QThread::finished, m_mpvController, &MpvVideoController::deleteLater);
    connect(this, &MpvVideoItem::observeProperty, m_mpvController, &MpvVideoController::observeProperty, Qt::QueuedConnection);
    connect(this, &MpvVideoItem::setProperty, m_mpvController, &MpvVideoController::setProperty, Qt::QueuedConnection);
    connect(this, &MpvVideoItem::command, m_mpvController, &MpvVideoController::command, Qt::QueuedConnection);

    observeProperty(MpvVideoItem::toString(MediaTitle), MPV_FORMAT_STRING);
    observeProperty(MpvVideoItem::toString(Position), MPV_FORMAT_DOUBLE);
    observeProperty(MpvVideoItem::toString(Duration), MPV_FORMAT_DOUBLE);
    observeProperty(MpvVideoItem::toString(Pause), MPV_FORMAT_FLAG);
    observeProperty(MpvVideoItem::toString(Volume), MPV_FORMAT_INT64);

    initConnections();

    setPropertyAsync(QStringLiteral("volume"), 0, static_cast<int>(AsyncIds::SetVolume));
    setProperty(QStringLiteral("mute"), true);
    getPropertyAsync(MpvVideoItem::toString(Volume), static_cast<int>(AsyncIds::GetVolume));
}

MpvVideoItem::~MpvVideoItem()
{
    if (m_mpvGL) {
        mpv_render_context_free(m_mpvGL);
    }
    mpv_set_wakeup_callback(m_mpv, nullptr, nullptr);

    m_workerThread->quit();
    m_workerThread->wait();
    m_workerThread->deleteLater();
    mpv_terminate_destroy(m_mpv);
}

QString MpvVideoItem::toString(Property p)
{
    switch (p) {
    case MediaTitle:  return QStringLiteral("media-title");
    case Position:    return QStringLiteral("time-pos");
    case Duration:    return QStringLiteral("duration");
    case Pause:       return QStringLiteral("pause");
    case Volume:      return QStringLiteral("volume");
    case Mute:        return QStringLiteral("mute");
    }
    return QString();
}

QString MpvVideoItem::mediaTitle()
{
    return getProperty(MpvVideoItem::toString(MediaTitle)).toString();
}

double MpvVideoItem::position()
{
    return getProperty(MpvVideoItem::toString(Position)).toDouble();
}

void MpvVideoItem::setPosition(double value)
{
    if (qFuzzyCompare(value, position())) {
        return;
    }
    Q_EMIT setPropertyAsync(MpvVideoItem::toString(Position), value);
}

double MpvVideoItem::duration()
{
    return getProperty(MpvVideoItem::toString(Duration)).toDouble();
}

QString MpvVideoItem::formattedPosition()
{
    return m_formattedPosition;
}

QString MpvVideoItem::formattedDuration()
{
    return m_formattedDuration;
}

bool MpvVideoItem::pause()
{
    return getProperty(MpvVideoItem::toString(Pause)).toBool();
}

void MpvVideoItem::setPause(bool value)
{
    if (value == pause()) {
        return;
    }

    Q_EMIT setPropertyAsync(MpvVideoItem::toString(Pause), value);
}

int MpvVideoItem::volume()
{
    return getProperty(MpvVideoItem::toString(Volume)).toInt();
}

void MpvVideoItem::setVolume(int value)
{
    if (value == volume()) {
        return;
    }
    Q_EMIT setPropertyAsync(MpvVideoItem::toString(Volume), value);
}

void MpvVideoItem::loadFile(const QString &file)
{
    auto url = QUrl::fromUserInput(file);
    if (m_source != url) {
        m_source = url;
        Q_EMIT currentUrlChanged();
    }

    Q_EMIT command(QStringList() << QStringLiteral("loadfile") << m_source.toString(QUrl::PreferLocalFile));
}

int MpvVideoItem::setPropertyBlocking(const QString &property, const QVariant &value)
{
    int error;
    QMetaObject::invokeMethod(m_mpvController,
                              "setProperty",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(int, error),
                              Q_ARG(QString, property),
                              Q_ARG(QVariant, value));

    return error;
}

void MpvVideoItem::setPropertyAsync(const QString &property, const QVariant &value, int id)
{

    QMetaObject::invokeMethod(m_mpvController,
                              "setPropertyAsync",
                              Qt::QueuedConnection,
                              Q_ARG(QString, property),
                              Q_ARG(QVariant, value),
                              Q_ARG(int, id));
}

QVariant MpvVideoItem::getProperty(const QString &property)
{
    QVariant value;
    QMetaObject::invokeMethod(m_mpvController,
                              "getProperty",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(QVariant, value),
                              Q_ARG(QString, property));

    return value;
}

void MpvVideoItem::getPropertyAsync(const QString &property, int id)
{
    QMetaObject::invokeMethod(m_mpvController,
                              "getPropertyAsync",
                              Qt::QueuedConnection,
                              Q_ARG(QString, property),
                              Q_ARG(int, id));
}

QVariant MpvVideoItem::commandBlocking(const QVariant &params)
{
    QVariant value;
    QMetaObject::invokeMethod(m_mpvController,
                              "command",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(QVariant, value),
                              Q_ARG(QVariant, params));
    return value;
}

void MpvVideoItem::commandAsync(const QStringList &params, int id)
{
    QMetaObject::invokeMethod(m_mpvController,
                              "commandAsync",
                              Qt::QueuedConnection,
                              Q_ARG(QVariant, params),
                              Q_ARG(int, id));
}

QVariant MpvVideoItem::expandText(const QString &text)
{
    QVariant value;
    QMetaObject::invokeMethod(m_mpvController,
                              "command",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(QVariant, value),
                              Q_ARG(QVariant, QVariant::fromValue(QStringList{QStringLiteral("expand-text"), text})));
    return value;
}

int MpvVideoItem::unobserveProperty(uint64_t id)
{
    return m_mpvController->unobserveProperty(id);
}

void MpvVideoItem::onPropertyChanged(const QString &property, const QVariant &value)
{
    if (property == toString(MediaTitle)) {
        Q_EMIT mediaTitleChanged();
    } else if (property == toString(Position)) {
        m_formattedPosition = formatTime(value.toDouble());
        Q_EMIT positionChanged();
    } else if (property == toString(Duration)) {
        m_formattedDuration = formatTime(value.toDouble());
        Q_EMIT durationChanged();
    } else if (property == toString(Pause)) {
        Q_EMIT pauseChanged();
    } else if (property == toString(Volume)) {
        Q_EMIT volumeChanged();
    }
}

void MpvVideoItem::onAsyncReply(const QVariant &data, mpv_event event)
{
    switch (static_cast<AsyncIds>(event.reply_userdata)) {
    case AsyncIds::None: {
        break;
    }
    case AsyncIds::SetVolume: {
        qCDebug(WALLPAPER) << "onSetPropertyReply" << event.reply_userdata;
        break;
    }
    case AsyncIds::GetVolume: {
        qCDebug(WALLPAPER) << "onGetPropertyReply" << event.reply_userdata << data;
        break;
    }
    case AsyncIds::ExpandText: {
        qCDebug(WALLPAPER) << "onGetPropertyReply" << event.reply_userdata << data;
        break;
    }
    }
}

void MpvVideoItem::initConnections()
{
    connect(m_mpvController, &MpvVideoController::propertyChanged,
            this, &MpvVideoItem::onPropertyChanged, Qt::QueuedConnection);
    connect(m_mpvController, &MpvVideoController::fileStarted,
            this, &MpvVideoItem::fileStarted, Qt::QueuedConnection);
    connect(m_mpvController, &MpvVideoController::fileLoaded,
            this, &MpvVideoItem::fileLoaded, Qt::QueuedConnection);
    connect(m_mpvController, &MpvVideoController::endFile,
            this, &MpvVideoItem::endFile, Qt::QueuedConnection);
    connect(m_mpvController, &MpvVideoController::videoReconfig,
            this, &MpvVideoItem::videoReconfig, Qt::QueuedConnection);
    connect(m_mpvController, &MpvVideoController::asyncReply,
            this, &MpvVideoItem::onAsyncReply, Qt::QueuedConnection);
}

QString MpvVideoItem::formatTime(const double time)
{
    int totalNumberOfSeconds = static_cast<int>(time);
    int seconds = totalNumberOfSeconds % 60;
    int minutes = (totalNumberOfSeconds / 60) % 60;
    int hours = (totalNumberOfSeconds / 60 / 60);

    QString timeString =
        QStringLiteral("%1:%2:%3").arg(hours, 2, 10, QLatin1Char('0')).arg(minutes, 2, 10, QLatin1Char('0')).arg(seconds, 2, 10, QLatin1Char('0'));

    return timeString;
}

QQuickFramebufferObject::Renderer *MpvVideoItem::createRenderer() const
{
    return new MpvRenderer(const_cast<MpvVideoItem *>(this));
}
