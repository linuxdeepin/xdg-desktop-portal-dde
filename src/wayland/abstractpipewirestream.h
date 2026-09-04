// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "portalcommon.h"
#include "protocols/imagecopycapture.h"
#include "screencastcontext.h"

#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QVector>

#include <array>
#include <memory>
#include <optional>
#include <sys/types.h>

class AbstractPipeWireStream : public QObject
{
    Q_OBJECT

public:
    AbstractPipeWireStream(QPointer<ScreenCastContext> context,
                           PortalCommon::CursorModes mode,
                           QObject *parent = nullptr);
    ~AbstractPipeWireStream() override;

    virtual int startScreencast() = 0;

    uint32_t nodeId() const
    {
        return m_nodeId;
    }

    void onStreamStateChanged(pw_stream_state oldState,
                              pw_stream_state state,
                              const char *error);
    void onStreamParamChanged(uint32_t id, const spa_pod *param);
    void onStreamRemoveBuffer(pw_buffer *buffer);
    void onStreamAddBuffer(pw_buffer *buffer);
    void onStreamProcess();

Q_SIGNALS:
    void ready(uint32_t nodeId);
    void failed(const QString &error);
    void closed(uint32_t nodeId);

protected:
    bool initializeCaptureSession(ext_image_capture_source_v1 *source);
    void setMaxFramerate(uint32_t framerate);
    void closeForSourceLoss(const QString &reason);
    void teardown();

    QPointer<ScreenCastContext> m_context;

private:
    enum class BufferTransport {
        Shm,
        DmaBuf,
    };

    struct DmaBufDevice {
        ~DmaBufDevice();

        dev_t deviceId = 0;
        int fd = -1;
        gbm_device *gbm = nullptr;
    };

    struct ShmFormat {
        uint32_t wlFormat = 0;
        uint32_t drmFormat = 0;
        spa_video_format spaFormat = SPA_VIDEO_FORMAT_UNKNOWN;
        uint32_t bytesPerPixel = 0;
    };

    struct DmaBufFormat {
        uint32_t drmFormat = 0;
        spa_video_format spaFormat = SPA_VIDEO_FORMAT_UNKNOWN;
        QVector<uint64_t> modifiers;
    };

    struct CaptureConstraints {
        uint32_t width = 0;
        uint32_t height = 0;
        QVector<ShmFormat> shmFormats;
        std::optional<dev_t> dmaBufDevice;
        QVector<DmaBufFormat> dmaBufFormats;
        uint64_t generation = 0;
    };

    struct NegotiatedFormat {
        BufferTransport transport = BufferTransport::Shm;
        uint32_t wlFormat = 0;
        uint32_t drmFormat = 0;
        spa_video_format spaFormat = SPA_VIDEO_FORMAT_UNKNOWN;
        uint32_t bytesPerPixel = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        uint64_t modifier = 0;
        uint32_t planeCount = 1;
        uint64_t constraintsGeneration = 0;
        std::shared_ptr<DmaBufDevice> dmaBufDevice;
    };

    struct BufferPlane {
        int fd = -1;
        uint32_t maxSize = 0;
        uint32_t stride = 0;
        uint32_t offset = 0;
    };

    struct PipeWireSourceBuffer {
        wl_buffer *waylandBuffer = nullptr;
        gbm_bo *gbmBo = nullptr;
        std::shared_ptr<DmaBufDevice> dmaBufDevice;
        std::array<BufferPlane, GBM_MAX_PLANES> planes;
        BufferTransport transport = BufferTransport::Shm;
        uint32_t planeCount = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t wlFormat = 0;
        uint32_t drmFormat = 0;
        uint64_t modifier = 0;
        uint64_t constraintsGeneration = 0;
        uint64_t bufferGeneration = 0;
    };

    struct FrameTransaction {
        uint64_t id = 0;
        pw_buffer *pipeWireBuffer = nullptr;
        PipeWireSourceBuffer *sourceBuffer = nullptr;
        ImageCopyCaptureFrame *captureFrame = nullptr;
        uint64_t presentationSeconds = 0;
        uint32_t presentationNanoseconds = 0;
        uint32_t transform = WL_OUTPUT_TRANSFORM_NORMAL;
        bool timestampReceived = false;
        bool transformReceived = false;
    };

    enum class LifecycleState {
        Initializing,
        Paused,
        Streaming,
        Stopping,
        Failed,
    };

    void beginConstraintBatch();
    bool validateConstraints(const CaptureConstraints &constraints, QString *error) const;
    bool constraintsEqual(const CaptureConstraints &lhs, const CaptureConstraints &rhs) const;
    void applyPendingConstraints();
    void schedulePendingConstraints();

    void handleCaptureSessionBufferSizeChanged(uint32_t width, uint32_t height);
    void handleCaptureSessionShmFormatChanged(uint32_t format);
    void handleCaptureSessionDmaBufDeviceChanged(wl_array *device);
    void handleCaptureSessionDmaBufFormatChanged(uint32_t format, wl_array *modifiers);
    void handleCaptureSessionDone();
    void handleCaptureSessionStopped();

    void startFrameCapture();
    void handleFrameTransform(uint32_t transform);
    void handleFramePresentationTime(uint32_t tvSecHi, uint32_t tvSecLo, uint32_t tvNsec);
    void handleFrameReady();
    void handleFrameFailed(uint32_t reason);
    void destroyCaptureFrame();
    void cancelTransaction(const char *reason);
    void finishTransaction(bool validFrame, const char *reason);

    bool createStream();
    void destroyStream();
    bool updateStreamFormats();
    bool updateStreamBufferParams();
    bool beginBufferReconfiguration(const char *reason);
    QVector<QByteArray> buildStreamFormatPods() const;
    bool isCurrentNegotiatedFormat() const;

    void refreshDmaBufCapabilities();
    std::shared_ptr<DmaBufDevice> createDmaBufDevice(dev_t deviceId) const;
    gbm_bo *allocateDmaBufBo(const std::shared_ptr<DmaBufDevice> &device,
                            uint32_t width,
                            uint32_t height,
                            uint32_t drmFormat,
                            uint64_t modifier) const;
    std::optional<NegotiatedFormat> selectDmaBufFormat(
            spa_video_format spaFormat,
            const QVector<uint64_t> &modifiers) const;
    bool rejectDmaBufModifiers(spa_video_format spaFormat,
                               const QVector<uint64_t> &modifiers);

    PipeWireSourceBuffer *createPipeWireSourceBuffer() const;
    PipeWireSourceBuffer *createShmPipeWireSourceBuffer() const;
    PipeWireSourceBuffer *createDmaBufPipeWireSourceBuffer() const;
    void destroyPipeWireSourceBuffer(PipeWireSourceBuffer *buffer) const;
    bool isCurrentBuffer(const PipeWireSourceBuffer *buffer) const;
    void queueBuffer(pw_buffer *pipeWireBuffer,
                     PipeWireSourceBuffer *sourceBuffer,
                     bool validFrame,
                     uint64_t pts,
                     uint32_t transform);

    uint64_t monotonicTimeNanoseconds() const;
    uint64_t transactionPresentationTime(bool *valid) const;
    uint64_t normalizePresentationTime(uint64_t pts);
    bool isTerminalState() const;
    void failStream(const QString &error);
    void emitClosed();

    QList<PipeWireSourceBuffer *> m_buffers;
    std::shared_ptr<PipeWireCore> m_pipeWireCore;

    ImageCopyCaptureSession *m_session = nullptr;
    ext_image_capture_source_v1 *m_source = nullptr;

    CaptureConstraints m_activeConstraints;
    CaptureConstraints m_constraintBatch;
    std::optional<CaptureConstraints> m_pendingConstraints;
    bool m_pendingConstraintsRequireReconfiguration = false;
    bool m_collectingConstraints = false;
    bool m_reconfiguringBuffers = false;
    bool m_constraintsApplyScheduled = false;
    uint64_t m_nextConstraintGeneration = 1;
    uint64_t m_bufferGeneration = 1;

    QVector<DmaBufFormat> m_usableDmaBufFormats;
    std::shared_ptr<DmaBufDevice> m_dmaBufDevice;
    std::optional<NegotiatedFormat> m_pendingDmaBufSelection;
    std::optional<NegotiatedFormat> m_negotiatedFormat;
    std::optional<FrameTransaction> m_transaction;

    pw_stream *m_stream = nullptr;
    spa_hook m_streamListener = {};
    spa_video_info_raw m_videoInfo = {};

    LifecycleState m_state = LifecycleState::Initializing;
    PortalCommon::CursorModes m_cursorMode;
    uint32_t m_nodeId = SPA_ID_INVALID;
    uint32_t m_maxFramerate = 60;
    uint32_t m_sequence = 0;
    uint64_t m_nextTransactionId = 1;
    uint64_t m_lastPresentationTime = 0;
    bool m_hasLastPresentationTime = false;
    bool m_readyEmitted = false;
    bool m_failureEmitted = false;
    bool m_closedEmitted = false;
};
