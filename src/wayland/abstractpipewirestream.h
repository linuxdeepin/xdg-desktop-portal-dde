// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "protocols/imagecapturesource.h"
#include "protocols/imagecopycapture.h"
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

#define XDPW_PWR_BUFFERS 2
#define XDPW_PWR_BUFFERS_MIN 2
#define XDPW_PWR_ALIGN 16

enum FrameState {
    XDPW_FRAME_STATE_NONE,
    XDPW_FRAME_STATE_STARTED,
    XDPW_FRAME_STATE_RENEG,
    XDPW_FRAME_STATE_FAILED,
    XDPW_FRAME_STATE_SUCCESS,
};

struct PipeWireSourceBuffer {
    QRegion damage;
    struct wl_buffer *buffer = nullptr;

    int fd[GBM_MAX_PLANES];
    uint32_t size[GBM_MAX_PLANES];
    uint32_t stride[GBM_MAX_PLANES];
    uint32_t offset[GBM_MAX_PLANES];

    uint32_t width;
    uint32_t height;
    uint32_t format;
    int planeCount;

    enum PortalCommon::BufferType bufferType;
};

struct PipeWireFrame {
    struct PipeWireSourceBuffer *pipeWireSourceBuffer = nullptr;
    struct pw_buffer *pwBuffer = nullptr;
    uint64_t tv_sec;
    uint32_t tv_nsec;
    uint32_t transformation;
};

class AbstractPipeWireStream : public QObject
{
    Q_OBJECT
public:
    struct xdpw_shm_format {
        uint32_t fourcc;
        uint32_t stride;
    };

    struct PipewireBufferConstraints {
        QList<struct DRMFormatModifierPair *> dmabuf_format_modifier_pairs;
        QList<struct xdpw_shm_format *> shm_formats;
        struct gbm_device *gbm = nullptr;
        uint32_t width, height;
        bool dirty = false;
    };

    AbstractPipeWireStream(QPointer<ScreenCastContext> context,
                           PortalCommon::CursorModes mode,
                           QObject *parent = nullptr);
    ~AbstractPipeWireStream() override;

    void onStreamStateChanged(enum pw_stream_state old,
                              enum pw_stream_state state,
                              const char *error);
    void onStreamParamChanged(uint32_t id, const struct spa_pod *param);
    void onStreamRemoveBuffer(struct pw_buffer *buffer);
    void onStreamAddBuffer(struct pw_buffer *buffer);
    void onStreamProcess();

    void enqueueBuffer();
    void dequeueBuffer();
    void buildFormats(struct spa_pod_builder *builder,
                       struct wl_array *params);

    uint32_t nodeId() const { return m_nodeId; }

    void pipewireBufferConstraintsInit(struct PipewireBufferConstraints *constraints);
    bool pipewireBufferConstraintsMove(struct PipewireBufferConstraints *dst, struct PipewireBufferConstraints *src);
    void pipewireBufferConstraintsFinish(struct PipewireBufferConstraints *constraints);

    void frameCapture();

    virtual int startScreencast() = 0;
    virtual void startframeCapture() = 0;
    void createImageCaptureFrame();

    void destroyImageCaptureFrame();
    void finishImageCaptureFrame();
Q_SIGNALS:
    void ready(uint32_t nodeId);
    void created(quint32 nodeid);
    void failed(const QString &error);
    void closed(uint32_t nodeId);

public Q_SLOTS:
    void handleCaptureSessionBufferSizeChanged(uint32_t width, uint32_t height);
    void handleCaptureSessionShmFormatChanged(uint32_t format);
    void handleCaptureSessionDmabufDeviceChanged(wl_array *device);
    void handleCaptureSessionDmabufFormatChanged(uint32_t format, wl_array *modifiers);
    void handleCaptureSessionDone();
    void handleCaptureSessionStopped();

    void handleFrameTransform(uint32_t transform);
    void handleFrameDamage(int32_t x, int32_t y, int32_t width, int32_t height);
    void handleFramePresentationTime(uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec);
    void handleFrameReady();
    void handleFrameFailed(uint32_t reason);

protected:
    void updateStreamParam();
    void createStream();
    void destroyStream();

private:
    void buildModifierList(uint32_t drm_format, uint64_t **modifiers, uint32_t *modifier_count);
    void queryDmabufModifiers(uint32_t drm_format,
                                     uint64_t *modifiers, uint32_t num_modifiers);
    uint32_t countDmabufModifiers(uint32_t drm_format);
    PipeWireSourceBuffer *createPipeWireSourceBuffer(enum PortalCommon::BufferType bufferType);
    void destroyPipeWireSourceBuffer(PipeWireSourceBuffer *buffer);
    bool hasDrmFourcc(uint32_t format);

protected:
    QList<PipeWireSourceBuffer *> m_buffers;

    QPointer<ScreenCastContext> m_context = nullptr;
    ImageCopyCaptureSession *m_session = nullptr;
    struct ::ext_image_capture_source_v1 *m_source = nullptr;
    ImageCopyCaptureFrame *m_frame = nullptr;

    struct PipewireBufferConstraints m_currentConstraints;
    struct PipewireBufferConstraints m_pendingConstraints;

    struct PipeWireFrame m_currentFrame;

    struct pw_stream *m_stream = nullptr;
    struct spa_hook m_streamListener;
    struct spa_video_info_raw m_pipewireVideoInfo;

    struct fps_limit_state fps_limit;

    uint32_t m_nodeId = SPA_ID_INVALID;
    uint32_t m_seq = 0;
    uint32_t m_framerate = 0;

    bool m_initialized = false;
    bool m_avoidDMAbufs = false;
    int m_err = 0;
    bool m_quit = false;
    bool m_isStreaming = false;

    PortalCommon::BufferType m_bufferType;
    FrameState m_frameState = XDPW_FRAME_STATE_NONE;
    PortalCommon::CursorModes m_mode;
};
