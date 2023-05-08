// Copyright © 2018 Red Hat, Inc
// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef SCREEN_CAST_STREAM_H
#define SCREEN_CAST_STREAM_H

#include <QObject>
#include <QSize>

#include <glib-object.h>

#include <pipewire/version.h>

#if !PW_CHECK_VERSION(0, 2, 9)
#include <spa/support/type-map.h>
#include <spa/param/format-utils.h>
#include <spa/param/video/raw-utils.h>
#endif
#include <spa/param/video/format-utils.h>
#include <spa/param/props.h>

#include <pipewire/factory.h>
#include <pipewire/pipewire.h>
#include <pipewire/remote.h>
#include <pipewire/stream.h>

#if !PW_CHECK_VERSION(0, 2, 9)
class PwType {
public:
  spa_type_media_type media_type;
  spa_type_media_subtype media_subtype;
  spa_type_format_video format_video;
  spa_type_video_format video_format;
};
#endif

class QSocketNotifier;

class ScreenCastStream : public QObject
{
    Q_OBJECT
public:
    explicit ScreenCastStream(const QSize &resolution, QObject *parent = nullptr);
    ~ScreenCastStream();

    // Public
    void init();
    uint framerate();
    uint nodeId();

    // Public because we need access from static functions
    bool createStream();
    void removeStream();

public Q_SLOTS:
    bool recordFrame(uint8_t *screenData);

Q_SIGNALS:
    void streamReady(uint nodeId);
    void startStreaming();
    void stopStreaming();

#if !PW_CHECK_VERSION(0, 2, 9)
private:
    void initializePwTypes();
#endif

private Q_SLOTS:
    void processPipewireEvents();

public:
#if PW_CHECK_VERSION(0, 2, 9)
    struct pw_core *pwCore = nullptr;
    struct pw_loop *pwLoop = nullptr;
    struct pw_stream *pwStream = nullptr;
    struct pw_remote *pwRemote = nullptr;
#else
    pw_core *pwCore = nullptr;
    pw_loop *pwLoop = nullptr;
    pw_stream *pwStream = nullptr;
    pw_remote *pwRemote = nullptr;
    pw_type *pwCoreType = nullptr;
    PwType *pwType = nullptr;
#endif

    spa_hook remoteListener;
    spa_hook streamListener;

    QSize resolution;
    QScopedPointer<QSocketNotifier> socketNotifier;

    spa_video_info_raw videoFormat;

};

#endif // SCREEN_CAST_STREAM_H


