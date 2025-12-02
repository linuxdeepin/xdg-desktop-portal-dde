// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "mpvvideocontroller.h"

#include <mpv/client.h>
#include <mpv/render_gl.h>

#include <QThread>
#include <QQuickItem>
#include <QQuickFramebufferObject>

class MpvVideoItem;

class MpvRenderer : public QQuickFramebufferObject::Renderer
{
public:
    explicit MpvRenderer(MpvVideoItem *item);
    ~MpvRenderer() = default;

    MpvVideoItem *m_item = nullptr;
    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override;
    void render() override;
};

class MpvVideoItem : public QQuickFramebufferObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString mediaTitle READ mediaTitle NOTIFY mediaTitleChanged)
    Q_PROPERTY(double position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(double duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(QString formattedPosition READ formattedPosition NOTIFY positionChanged)
    Q_PROPERTY(QString formattedDuration READ formattedDuration NOTIFY durationChanged)
    Q_PROPERTY(bool pause READ pause WRITE setPause NOTIFY pauseChanged)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)

public:
    explicit MpvVideoItem(QQuickItem *parent = nullptr);
    ~MpvVideoItem() override;

    enum class AsyncIds {
        None,
        SetVolume,
        GetVolume,
        ExpandText,
    };
    Q_ENUM(AsyncIds)

    enum Property {
        MediaTitle,
        Position,
        Duration,
        Pause,
        Volume,
        Mute
    };
    Q_ENUM(Property)

    static QString toString(Property p);

    Renderer *createRenderer() const override;

    QString mediaTitle();

    double position();
    void setPosition(double value);

    double duration();

    QString formattedPosition();
    QString formattedDuration();

    bool pause();
    void setPause(bool value);

    int volume();
    void setVolume(int value);

    Q_INVOKABLE void loadFile(const QString &file);

    Q_INVOKABLE int setPropertyBlocking(const QString &property, const QVariant &value);
    Q_INVOKABLE void setPropertyAsync(const QString &property, const QVariant &value, int id = 0);
    Q_INVOKABLE QVariant getProperty(const QString &property);
    Q_INVOKABLE void getPropertyAsync(const QString &property, int id = 0);
    Q_INVOKABLE QVariant commandBlocking(const QVariant &params);
    Q_INVOKABLE void commandAsync(const QStringList &params, int id = 0);
    Q_INVOKABLE QVariant expandText(const QString &text);
    Q_INVOKABLE int unobserveProperty(uint64_t id);

Q_SIGNALS:
    void mediaTitleChanged();
    void currentUrlChanged();
    void positionChanged();
    void durationChanged();
    void pauseChanged();
    void volumeChanged();
    void sourceChanged();

    void fileStarted();
    void fileLoaded();
    void endFile(QString reason);
    void videoReconfig();

    void ready();
    void observeProperty(const QString &property, mpv_format format, uint64_t id = 0);
    void setProperty(const QString &property, const QVariant &value);
    void command(const QStringList &params);

private Q_SLOTS:
    void onPropertyChanged(const QString &property, const QVariant &value);
    void onAsyncReply(const QVariant &data, mpv_event event);

private:
   void initConnections();
   QString formatTime(const double time);

private:
    friend class MpvRenderer;

    QThread *m_workerThread = nullptr;
    MpvVideoController *m_mpvController = nullptr;
    mpv_handle *m_mpv = nullptr;
    mpv_render_context *m_mpvGL = nullptr;

    QString m_formattedPosition;
    QString m_formattedDuration;
    QUrl m_source;
};
