// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "wayland-wayland-client-protocol.h"
#include "screencastcontext.h"
#include "portalcommon.h"
#include "fpslimit.h"

#include <gbm.h>
#include <xf86drm.h>
#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>

#include <QObject>
#include <QPointer>
#include <QScreen>

#define XDPW_PWR_BUFFERS 2
#define XDPW_PWR_BUFFERS_MIN 2
#define XDPW_PWR_ALIGN 16

class ScreenCopyFrame;

class PipeWireStream : public QObject
{
    Q_OBJECT
public:
    enum FrameState {
        XDPW_FRAME_STATE_NONE,
        XDPW_FRAME_STATE_STARTED,
        XDPW_FRAME_STATE_RENEG,
        XDPW_FRAME_STATE_FAILED,
        XDPW_FRAME_STATE_SUCCESS,
    };

    struct PipeWireSourceBuffer {
        enum PortalCommon::BufferType bufferType;

        uint32_t width;
        uint32_t height;
        uint32_t format;
        int planeCount;

        int fd[4];
        uint32_t size[4];
        uint32_t stride[4];
        uint32_t offset[4];

        struct gbm_bo *bo = nullptr;
        struct wl_buffer *buffer = nullptr;
    };

    struct PipeWireFrame {
        bool y_invert;
        uint64_t tv_sec;
        uint32_t tv_nsec;
        uint32_t transformation;
        QRect damage[4];
        uint32_t damage_count;
        struct PipeWireSourceBuffer *PipeWireSourceBuffer = nullptr;
        struct pw_buffer *pw_buffer = nullptr;
    };

    PipeWireStream(QPointer<ScreenCastContext> context,
                   QScreen *output,
                   PortalCommon::CursorModes mode,
                   QObject *parent = nullptr);
    PipeWireStream(QPointer<ScreenCastContext> context,
                   const QRect &region,
                   PortalCommon::CursorModes mode,
                   QObject *parent = nullptr);
    ~PipeWireStream() override;

    PipeWireSourceBuffer *createPipeWireSourceBuffer(enum PortalCommon::BufferType bufferType,
                                                     ScreenCopyFrameInfo *frameInfo);
    void destroyPipeWireSourceBuffer(PipeWireSourceBuffer *buffer);
    uint32_t nodeId() const { return m_nodeId; }
    void enqueueBuffer();
    void dequeueBuffer();
    uint32_t buildFormats(struct spa_pod_builder *b[2],
                           const struct spa_pod *params[2]);
    void frameCapture();
    void createScreencopyFrame();
    int startScreencast();
    void onStreamStateChanged(enum pw_stream_state old,
                              enum pw_stream_state state,
                              const char *error);
    void onStreamParamChanged(uint32_t id, const struct spa_pod *param);
    void onStreamRemoveBuffer(struct pw_buffer *buffer);
    void onStreamAddBuffer(struct pw_buffer *buffer);
    void onStreamProcess();
public Q_SLOTS:
    void startframeCapture();

private:
    bool buildModifierlist(uint32_t drmFormat,
                            uint64_t **modifiers,
                            uint32_t *modifierCount);
    void screenCopyFrameFinish();
    void destroyScreenCopyFrame();
    void updateStreamParam();
    void createStream();
    void destroyStream();

Q_SIGNALS:
    void ready(uint32_t nodeId);
    void created(quint32 nodeid);
    void failed(const QString &error);
    void closed(uint32_t nodeId);

private Q_SLOTS:
    void handleFrameBuffer(uint32_t format,
                           uint32_t width,
                           uint32_t height,
                           uint32_t stride);
    void handleFrameFlags(uint32_t flags);
    void handleFrameReady(uint32_t tv_sec_hi,
                          uint32_t tv_sec_lo,
                          uint32_t tv_nsec);
    void handleFrameFailed();
    void handleFrameDamage(uint32_t x,
                           uint32_t y,
                           uint32_t width,
                           uint32_t height);
    void handleFrameLinuxDMABuf(uint32_t format,
                                uint32_t width,
                                uint32_t height);
    void handleFrameBufferDone();

private:
    QPointer<ScreenCastContext> m_context = nullptr;

    QScreen *m_output = nullptr;
    PortalCommon::SourceType m_sourceTYpe;
    PortalCommon::CursorModes m_mode;
    QPointer<ScreenCopyFrame> m_frame = nullptr;
    QRegion m_copyRegion;

    struct PipeWireFrame m_currentFrame;
    struct pw_stream *m_stream = nullptr;
    struct spa_hook m_streamListener;
    struct spa_video_info_raw m_pipewireVideoInfo;
    struct ScreenCopyFrameInfo m_screencopyFrameInfo[2];
    struct fps_limit_state fps_limit;

    uint32_t m_seq;
    uint32_t m_nodeId = SPA_ID_INVALID;
    uint32_t m_framerate;
    enum FrameState m_frameState = XDPW_FRAME_STATE_NONE;
    bool m_initialized = false;
    bool m_avoidDMAbufs = false;
    bool m_isStreaming = false;
    int m_err = 0;
    bool m_quit = false;
    enum PortalCommon::BufferType m_bufferType;
};
