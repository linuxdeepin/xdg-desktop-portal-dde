// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "abstractpipewirestream.h"

#include "loggings.h"
#include "pipewirecore.h"
#include "pipewireutils.h"
#include "sealedmemfd.h"

#include <drm_fourcc.h>
#include <fcntl.h>
#include <pipewire/stream.h>
#include <spa/buffer/meta.h>
#include <spa/param/format-utils.h>
#include <spa/param/props.h>
#include <spa/param/video/format.h>
#include <spa/pod/builder.h>
#include <spa/utils/result.h>
#include <xf86drm.h>

#include <QVarLengthArray>

#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <limits>
#include <unistd.h>

namespace {

constexpr uint32_t s_defaultBufferCount = 3;
constexpr uint32_t s_minimumBufferCount = 2;
constexpr uint32_t s_maximumBufferCount = 4;
constexpr uint32_t s_bufferAlignment = 16;
constexpr uint64_t s_nanosecondsPerSecond = 1000000000ULL;
constexpr qsizetype s_maxShmFormats = 64;
constexpr qsizetype s_maxDmaBufFormats = 64;
constexpr qsizetype s_maxDmaBufModifiersPerFormat = 256;

void handleStreamDestroy(void *data)
{
    Q_UNUSED(data);
    qCDebug(SCREENCAST) << "xdpw: PipeWire stream destroyed";
}

void handleStreamStateChanged(void *data,
                              pw_stream_state oldState,
                              pw_stream_state state,
                              const char *error)
{
    auto *stream = static_cast<AbstractPipeWireStream *>(data);
    stream->onStreamStateChanged(oldState, state, error);
}

void handleStreamParamChanged(void *data, uint32_t id, const spa_pod *param)
{
    auto *stream = static_cast<AbstractPipeWireStream *>(data);
    stream->onStreamParamChanged(id, param);
}

void handleStreamAddBuffer(void *data, pw_buffer *buffer)
{
    auto *stream = static_cast<AbstractPipeWireStream *>(data);
    stream->onStreamAddBuffer(buffer);
}

void handleStreamRemoveBuffer(void *data, pw_buffer *buffer)
{
    auto *stream = static_cast<AbstractPipeWireStream *>(data);
    stream->onStreamRemoveBuffer(buffer);
}

void handleStreamProcess(void *data)
{
    auto *stream = static_cast<AbstractPipeWireStream *>(data);
    stream->onStreamProcess();
}

const pw_stream_events s_streamEvents = [] {
    pw_stream_events events = {};
    events.version = PW_VERSION_STREAM_EVENTS;
    events.destroy = handleStreamDestroy;
    events.state_changed = handleStreamStateChanged;
    events.param_changed = handleStreamParamChanged;
    events.add_buffer = handleStreamAddBuffer;
    events.remove_buffer = handleStreamRemoveBuffer;
    events.process = handleStreamProcess;
    return events;
}();

std::optional<QByteArray> buildFormatPod(spa_video_format format,
                                         uint32_t width,
                                         uint32_t height,
                                         uint32_t maximumFramerate,
                                         const QVector<uint64_t> &modifiers,
                                         std::optional<uint64_t> fixedModifier = std::nullopt)
{
    if (modifiers.size() > s_maxDmaBufModifiersPerFormat) {
        return std::nullopt;
    }

    constexpr int baseStorageSize = 2048;
    constexpr int bytesPerModifier = 32;
    if (modifiers.size() > (INT_MAX - baseStorageSize) / bytesPerModifier) {
        return std::nullopt;
    }

    QByteArray storage(baseStorageSize
                               + static_cast<int>(modifiers.size()) * bytesPerModifier,
                       '\0');
    spa_pod_builder builder = SPA_POD_BUILDER_INIT(
            storage.data(), static_cast<uint32_t>(storage.size()));
    spa_pod_frame frame;
    spa_pod_builder_push_object(&builder, &frame, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat);
    spa_pod_builder_add(&builder,
                        SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
                        SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
                        SPA_FORMAT_VIDEO_format, SPA_POD_Id(format),
                        0);

    const spa_rectangle size = SPA_RECTANGLE(width, height);
    const spa_fraction variableFramerate = SPA_FRACTION(0, 1);
    const spa_fraction minimumFramerate = SPA_FRACTION(1, 1);
    const spa_fraction maximum = SPA_FRACTION(maximumFramerate, 1);
    spa_pod_builder_add(&builder,
                        SPA_FORMAT_VIDEO_size, SPA_POD_Rectangle(&size),
                        SPA_FORMAT_VIDEO_framerate, SPA_POD_Fraction(&variableFramerate),
                        SPA_FORMAT_VIDEO_maxFramerate,
                        SPA_POD_CHOICE_RANGE_Fraction(&maximum, &minimumFramerate, &maximum),
                        0);

    if (fixedModifier) {
        spa_pod_builder_prop(&builder,
                             SPA_FORMAT_VIDEO_modifier,
                             SPA_POD_PROP_FLAG_MANDATORY);
        spa_pod_builder_long(&builder, static_cast<int64_t>(*fixedModifier));
    } else if (!modifiers.isEmpty()) {
        spa_pod_frame modifierFrame;
        spa_pod_builder_prop(&builder,
                             SPA_FORMAT_VIDEO_modifier,
                             SPA_POD_PROP_FLAG_MANDATORY
                                     | SPA_POD_PROP_FLAG_DONT_FIXATE);
        spa_pod_builder_push_choice(&builder, &modifierFrame, SPA_CHOICE_Enum, 0);
        spa_pod_builder_long(&builder, static_cast<int64_t>(modifiers.first()));
        for (uint64_t modifier : modifiers) {
            spa_pod_builder_long(&builder, static_cast<int64_t>(modifier));
        }
        spa_pod_builder_pop(&builder, &modifierFrame);
    }

    spa_pod *pod = static_cast<spa_pod *>(spa_pod_builder_pop(&builder, &frame));
    if (!pod || spa_pod_builder_corrupted(&builder)
        || SPA_POD_SIZE(pod) > static_cast<uint64_t>(INT_MAX)) {
        return std::nullopt;
    }

    storage.resize(static_cast<int>(SPA_POD_SIZE(pod)));
    return storage;
}

spa_pod *buildBufferParam(spa_pod_builder *builder,
                          uint32_t blocks,
                          uint32_t dataType,
                          uint32_t size = 0,
                          uint32_t stride = 0)
{
    spa_pod_frame frame;
    spa_pod_builder_push_object(builder, &frame, SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers);
    spa_pod_builder_add(builder,
                        SPA_PARAM_BUFFERS_buffers,
                        SPA_POD_CHOICE_RANGE_Int(s_defaultBufferCount,
                                                 s_minimumBufferCount,
                                                 s_maximumBufferCount),
                        SPA_PARAM_BUFFERS_blocks, SPA_POD_Int(blocks),
                        SPA_PARAM_BUFFERS_align, SPA_POD_Int(s_bufferAlignment),
                        SPA_PARAM_BUFFERS_dataType,
                        SPA_POD_CHOICE_FLAGS_Int(dataType),
                        0);
    if (size > 0) {
        spa_pod_builder_add(builder,
                            SPA_PARAM_BUFFERS_size, SPA_POD_Int(size),
                            SPA_PARAM_BUFFERS_stride, SPA_POD_Int(stride),
                            0);
    }
    return static_cast<spa_pod *>(spa_pod_builder_pop(builder, &frame));
}

std::optional<QVector<uint64_t>> modifierValues(const spa_pod_prop *property)
{
    if (!property) {
        return std::nullopt;
    }

    QVector<uint64_t> result;
    const spa_pod *value = &property->value;
    if (SPA_POD_CHECK(value, SPA_TYPE_Long, sizeof(int64_t))) {
        int64_t modifier = 0;
        std::memcpy(&modifier, SPA_POD_BODY_CONST(value), sizeof(modifier));
        result.append(static_cast<uint64_t>(modifier));
        return result;
    }

    if (!SPA_POD_CHECK(value, SPA_TYPE_Choice, sizeof(spa_pod_choice_body))
        || SPA_POD_CHOICE_VALUE_TYPE(value) != SPA_TYPE_Long
        || SPA_POD_CHOICE_VALUE_SIZE(value) != sizeof(int64_t)) {
        return std::nullopt;
    }

    const size_t valueCount = SPA_POD_CHOICE_N_VALUES(value);
    if (valueCount == 0
        || valueCount > static_cast<size_t>(s_maxDmaBufModifiersPerFormat + 1)) {
        return std::nullopt;
    }

    const auto *values = static_cast<const int64_t *>(SPA_POD_CHOICE_VALUES(value));
    result.reserve(static_cast<qsizetype>(valueCount));
    for (size_t index = 0; index < valueCount; ++index) {
        const uint64_t modifier = static_cast<uint64_t>(values[index]);
        if (!result.contains(modifier)) {
            result.append(modifier);
        }
    }
    return result;
}

QString frameFailureReason(uint32_t reason)
{
    switch (reason) {
    case EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_UNKNOWN:
        return QStringLiteral("unknown runtime error");
    case EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_BUFFER_CONSTRAINTS:
        return QStringLiteral("buffer constraints changed");
    case EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_STOPPED:
        return QStringLiteral("capture session stopped");
    default:
        return QStringLiteral("undefined failure reason %1").arg(reason);
    }
}

QVector<spa_video_format> compatibleShmSpaFormats(spa_video_format format)
{
    if (format == SPA_VIDEO_FORMAT_UNKNOWN) {
        return {};
    }

    QVector<spa_video_format> result{format};
    const spa_video_format withoutAlpha =
            PipeWireutils::pipewireFormatStripAlpha(format);
    if (withoutAlpha != SPA_VIDEO_FORMAT_UNKNOWN
        && withoutAlpha != format) {
        result.append(withoutAlpha);
    }
    return result;
}

}

AbstractPipeWireStream::DmaBufDevice::~DmaBufDevice()
{
    if (gbm) {
        gbm_device_destroy(gbm);
        gbm = nullptr;
    }
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
}

AbstractPipeWireStream::AbstractPipeWireStream(QPointer<ScreenCastContext> context,
                                               PortalCommon::CursorModes mode,
                                               QObject *parent)
    : QObject(parent)
    , m_context(context)
    , m_cursorMode(mode)
{
    if (m_context) {
        m_pipeWireCore = m_context->pipeWireCore();
    }
    if (m_pipeWireCore) {
        connect(m_pipeWireCore.get(),
                &PipeWireCore::pipewireFailed,
                this,
                [this](const QString &message) {
            failStream(QStringLiteral("PipeWire core connection failed: %1")
                               .arg(message.isEmpty()
                                            ? QStringLiteral("connection closed")
                                            : message));
        });
    }
}

AbstractPipeWireStream::~AbstractPipeWireStream()
{
    teardown();
}

void AbstractPipeWireStream::teardown()
{
    m_state = LifecycleState::Stopping;
    cancelTransaction("stream teardown");
    destroyStream();

    if (m_session) {
        QObject::disconnect(m_session, nullptr, this, nullptr);
        m_session->destroy();
        m_session = nullptr;
    }
    if (m_source) {
        ext_image_capture_source_v1_destroy(m_source);
        m_source = nullptr;
    }

    for (PipeWireSourceBuffer *buffer : std::as_const(m_buffers)) {
        destroyPipeWireSourceBuffer(buffer);
    }
    m_buffers.clear();
    m_negotiatedFormat.reset();
    m_pendingDmaBufSelection.reset();
    m_usableDmaBufFormats.clear();
    m_dmaBufDevice.reset();
}

bool AbstractPipeWireStream::initializeCaptureSession(ext_image_capture_source_v1 *source)
{
    if (!source) {
        qCWarning(SCREENCAST) << "xdpw: cannot initialize a null image-copy source";
        return false;
    }

    m_source = source;
    if (!m_context || !m_context->m_imageCopyCaptureManager
        || !m_context->m_imageCopyCaptureManager->isActive()) {
        qCWarning(SCREENCAST) << "xdpw: cannot initialize image-copy session";
        return false;
    }

    const uint32_t options = m_cursorMode == PortalCommon::Embedded
            ? EXT_IMAGE_COPY_CAPTURE_MANAGER_V1_OPTIONS_PAINT_CURSORS
            : 0;
    ext_image_copy_capture_session_v1 *sessionObject =
            m_context->m_imageCopyCaptureManager->create_session(m_source, options);
    if (!sessionObject) {
        qCWarning(SCREENCAST) << "xdpw: compositor did not create an image-copy session";
        return false;
    }

    m_session = new ImageCopyCaptureSession(sessionObject, this);
    connect(m_session, &ImageCopyCaptureSession::bufferSizeChanged,
            this, &AbstractPipeWireStream::handleCaptureSessionBufferSizeChanged);
    connect(m_session, &ImageCopyCaptureSession::shmFormatChanged,
            this, &AbstractPipeWireStream::handleCaptureSessionShmFormatChanged);
    connect(m_session, &ImageCopyCaptureSession::dmabufDeviceChanged,
            this, &AbstractPipeWireStream::handleCaptureSessionDmaBufDeviceChanged);
    connect(m_session, &ImageCopyCaptureSession::dmabufFormatChanged,
            this, &AbstractPipeWireStream::handleCaptureSessionDmaBufFormatChanged);
    connect(m_session, &ImageCopyCaptureSession::done,
            this, &AbstractPipeWireStream::handleCaptureSessionDone);
    connect(m_session, &ImageCopyCaptureSession::stopped,
            this, &AbstractPipeWireStream::handleCaptureSessionStopped);

    qCInfo(SCREENCAST) << "xdpw: image-copy session created; waiting for initial constraints";
    return true;
}

void AbstractPipeWireStream::setMaxFramerate(uint32_t framerate)
{
    m_maxFramerate = std::clamp(framerate, 1U, 60U);
}

void AbstractPipeWireStream::closeForSourceLoss(const QString &reason)
{
    failStream(reason);
}

void AbstractPipeWireStream::onStreamStateChanged(pw_stream_state oldState,
                                                  pw_stream_state state,
                                                  const char *error)
{
    qCInfo(SCREENCAST) << "xdpw: PipeWire state"
                       << pw_stream_state_as_string(oldState)
                       << "->" << pw_stream_state_as_string(state)
                       << "transaction" << (m_transaction ? m_transaction->id : 0);

    // pw_stream_destroy() synchronously reports PAUSED and UNCONNECTED. Once
    // teardown or failure has started, those transport state changes must not
    // overwrite the terminal lifecycle state or emit a second failure.
    if (isTerminalState()) {
        qCDebug(SCREENCAST) << "xdpw: ignoring PipeWire state change in terminal state";
        return;
    }

    switch (state) {
    case PW_STREAM_STATE_ERROR:
        failStream(QStringLiteral("PipeWire stream error: %1")
                           .arg(QString::fromUtf8(error ? error : "unknown error")));
        break;
    case PW_STREAM_STATE_STREAMING:
        m_state = LifecycleState::Streaming;
        break;
    case PW_STREAM_STATE_PAUSED:
        m_state = LifecycleState::Paused;
        if (!m_readyEmitted && m_stream) {
            const uint32_t nodeId = pw_stream_get_node_id(m_stream);
            if (nodeId != SPA_ID_INVALID) {
                m_nodeId = nodeId;
                m_readyEmitted = true;
                qCInfo(SCREENCAST) << "xdpw: PipeWire node ready" << m_nodeId;
                Q_EMIT ready(m_nodeId);
            }
        }
        // PAUSED is a scheduling state. It never transfers ownership of an
        // in-flight image-copy buffer back to PipeWire.
        break;
    case PW_STREAM_STATE_UNCONNECTED:
        if (!m_failureEmitted) {
            failStream(QStringLiteral("PipeWire stream disconnected"));
        }
        break;
    case PW_STREAM_STATE_CONNECTING:
        break;
    }
}

void AbstractPipeWireStream::onStreamParamChanged(uint32_t id, const spa_pod *param)
{
    if (!param || id != SPA_PARAM_Format || isTerminalState()) {
        return;
    }

    spa_video_info_raw videoInfo = {};
    const int result = spa_format_video_raw_parse(param, &videoInfo);
    if (result < 0) {
        failStream(QStringLiteral("PipeWire selected an invalid video format"));
        return;
    }

    if (videoInfo.size.width != m_activeConstraints.width
        || videoInfo.size.height != m_activeConstraints.height) {
        // EnumFormat updates and the resulting Format callbacks are
        // asynchronous. A consumer can also trigger modifier renegotiation at
        // the same time as the compositor publishes a new capture size. In
        // that case PipeWire may still deliver the selection from the previous
        // EnumFormat after the newer constraints became active.
        //
        // Never allocate against that mixed state: reject the obsolete
        // selection by publishing the current exact-size formats again. This
        // is the normal PipeWire renegotiation response and keeps the latest
        // compositor constraints authoritative.
        qCInfo(SCREENCAST)
                << "xdpw: rejecting obsolete PipeWire frame size"
                << videoInfo.size.width << "x" << videoInfo.size.height
                << "while constraints generation"
                << m_activeConstraints.generation << "requires"
                << m_activeConstraints.width << "x" << m_activeConstraints.height;

        m_negotiatedFormat.reset();
        if (!m_reconfiguringBuffers) {
            if (!beginBufferReconfiguration(
                        "PipeWire selected an obsolete frame size")) {
                failStream(QStringLiteral(
                        "Failed to re-offer current capture dimensions"));
            }
        } else if (!updateStreamFormats()) {
            failStream(QStringLiteral(
                    "Failed to re-offer current capture dimensions"));
        }
        return;
    }

    const spa_pod_prop *modifierProperty =
            spa_pod_find_prop(param, nullptr, SPA_FORMAT_VIDEO_modifier);
    if (modifierProperty) {
        const std::optional<QVector<uint64_t>> modifiers =
                modifierValues(modifierProperty);
        if (!modifiers || modifiers->isEmpty()) {
            failStream(QStringLiteral("PipeWire selected an invalid DMA-BUF modifier list"));
            return;
        }

        std::optional<NegotiatedFormat> selection;
        if (m_pendingDmaBufSelection
            && m_pendingDmaBufSelection->spaFormat == videoInfo.format
            && m_pendingDmaBufSelection->width == m_activeConstraints.width
            && m_pendingDmaBufSelection->height == m_activeConstraints.height
            && m_pendingDmaBufSelection->constraintsGeneration
                    == m_activeConstraints.generation
            && m_pendingDmaBufSelection->dmaBufDevice == m_dmaBufDevice
            && modifiers->contains(m_pendingDmaBufSelection->modifier)) {
            // The fixed format is the exact allocation already tested during
            // the first DONT_FIXATE callback. Reusing it avoids an unnecessary
            // GBM allocation on the latency-sensitive stream startup path.
            selection = m_pendingDmaBufSelection;
        } else {
            selection = selectDmaBufFormat(videoInfo.format, *modifiers);
        }
        if (!selection) {
            m_pendingDmaBufSelection.reset();
            if (rejectDmaBufModifiers(videoInfo.format, *modifiers)
                && updateStreamFormats()) {
                qCWarning(SCREENCAST)
                        << "xdpw: rejected unusable DMA-BUF modifiers and retrying negotiation";
                return;
            }
            failStream(QStringLiteral(
                    "PipeWire selected a DMA-BUF format that cannot be allocated"));
            return;
        }

        const bool requiresFixation =
                (modifierProperty->flags & SPA_POD_PROP_FLAG_DONT_FIXATE) != 0;
        const bool samePendingSelection = m_pendingDmaBufSelection
                && m_pendingDmaBufSelection->drmFormat == selection->drmFormat
                && m_pendingDmaBufSelection->modifier == selection->modifier
                && m_pendingDmaBufSelection->planeCount == selection->planeCount
                && m_pendingDmaBufSelection->width == selection->width
                && m_pendingDmaBufSelection->height == selection->height
                && m_pendingDmaBufSelection->constraintsGeneration
                        == selection->constraintsGeneration
                && m_pendingDmaBufSelection->dmaBufDevice == selection->dmaBufDevice;
        if (requiresFixation && !samePendingSelection) {
            m_pendingDmaBufSelection = std::move(selection);
            m_videoInfo = videoInfo;
            m_videoInfo.modifier = m_pendingDmaBufSelection->modifier;
            qCInfo(SCREENCAST) << "xdpw: fixing DMA-BUF format"
                               << m_videoInfo.format
                               << "constraints generation"
                               << m_pendingDmaBufSelection->constraintsGeneration
                               << "modifier" << Qt::hex
                               << m_pendingDmaBufSelection->modifier
                               << Qt::dec
                               << "planes" << m_pendingDmaBufSelection->planeCount;
            if (!updateStreamFormats()) {
                failStream(QStringLiteral("Failed to fixate the DMA-BUF modifier"));
            }
            return;
        }

        if (requiresFixation) {
            // A fixed format was already placed first in EnumFormat. A few
            // PipeWire graph combinations can nevertheless echo the original
            // choice once more. Its preferred value is the value parsed into
            // videoInfo.modifier, so accepting it is safe only when it is the
            // exact allocation we already tested and advertised.
            if (videoInfo.modifier != selection->modifier) {
                failStream(QStringLiteral("PipeWire did not honor DMA-BUF fixation"));
                return;
            }
            qCWarning(SCREENCAST)
                    << "xdpw: accepting repeated DMA-BUF modifier choice after fixation";
        }

        m_videoInfo = videoInfo;
        m_videoInfo.modifier = selection->modifier;
        m_negotiatedFormat = std::move(selection);
        m_pendingDmaBufSelection.reset();
        qCInfo(SCREENCAST) << "xdpw: negotiated DMA-BUF format"
                           << m_videoInfo.format
                           << m_videoInfo.size.width << "x" << m_videoInfo.size.height
                           << "constraints generation"
                           << m_negotiatedFormat->constraintsGeneration
                           << "modifier" << Qt::hex
                           << m_negotiatedFormat->modifier
                           << Qt::dec
                           << "planes" << m_negotiatedFormat->planeCount;
    } else {
        auto formatIterator =
                std::find_if(m_activeConstraints.shmFormats.cbegin(),
                             m_activeConstraints.shmFormats.cend(),
                             [format = videoInfo.format](const ShmFormat &candidate) {
                                 return candidate.spaFormat == format;
                             });
        if (formatIterator == m_activeConstraints.shmFormats.cend()) {
            formatIterator =
                    std::find_if(m_activeConstraints.shmFormats.cbegin(),
                                 m_activeConstraints.shmFormats.cend(),
                                 [format = videoInfo.format](const ShmFormat &candidate) {
                return PipeWireutils::pipewireFormatStripAlpha(
                               candidate.spaFormat)
                        == format;
            });
        }
        if (formatIterator == m_activeConstraints.shmFormats.cend()) {
            failStream(QStringLiteral(
                    "PipeWire selected a format not offered by the compositor"));
            return;
        }

        NegotiatedFormat selection;
        selection.transport = BufferTransport::Shm;
        selection.wlFormat = formatIterator->wlFormat;
        selection.drmFormat = formatIterator->drmFormat;
        selection.spaFormat = videoInfo.format;
        selection.bytesPerPixel = formatIterator->bytesPerPixel;
        selection.width = m_activeConstraints.width;
        selection.height = m_activeConstraints.height;
        selection.constraintsGeneration = m_activeConstraints.generation;
        m_videoInfo = videoInfo;
        m_negotiatedFormat = std::move(selection);
        m_pendingDmaBufSelection.reset();
        qCInfo(SCREENCAST) << "xdpw: negotiated SHM format"
                           << m_videoInfo.format
                           << m_videoInfo.size.width << "x" << m_videoInfo.size.height
                           << "constraints generation"
                           << m_negotiatedFormat->constraintsGeneration;
    }

    if (!updateStreamBufferParams()) {
        failStream(QStringLiteral("Failed to configure PipeWire capture buffers"));
    }
}

void AbstractPipeWireStream::onStreamAddBuffer(pw_buffer *buffer)
{
    if (isTerminalState()) {
        return;
    }

    if (!isCurrentNegotiatedFormat() || !buffer || !buffer->buffer
        || !buffer->buffer->datas) {
        failStream(QStringLiteral("PipeWire supplied an invalid buffer layout"));
        return;
    }

    const uint32_t expectedPlaneCount = m_negotiatedFormat->planeCount;
    if (expectedPlaneCount == 0
        || expectedPlaneCount > GBM_MAX_PLANES
        || buffer->buffer->n_datas != expectedPlaneCount) {
        failStream(QStringLiteral("PipeWire supplied an unexpected plane layout"));
        return;
    }

    spa_data *data = buffer->buffer->datas;
    const spa_data_type dataType =
            m_negotiatedFormat->transport == BufferTransport::DmaBuf
            ? SPA_DATA_DmaBuf
            : SPA_DATA_MemFd;
    for (uint32_t plane = 0; plane < expectedPlaneCount; ++plane) {
        if ((data[plane].type & (1U << dataType)) == 0
            || !data[plane].chunk) {
            failStream(QStringLiteral(
                    "PipeWire supplied an incompatible buffer plane"));
            return;
        }
    }

    PipeWireSourceBuffer *sourceBuffer = createPipeWireSourceBuffer();
    if (!sourceBuffer) {
        failStream(m_negotiatedFormat->transport == BufferTransport::DmaBuf
                           ? QStringLiteral("Failed to allocate a DMA-BUF capture buffer")
                           : QStringLiteral("Failed to allocate a shared-memory capture buffer"));
        return;
    }
    if (sourceBuffer->planeCount != expectedPlaneCount) {
        destroyPipeWireSourceBuffer(sourceBuffer);
        failStream(QStringLiteral("Allocated capture buffer has an unexpected plane count"));
        return;
    }

    for (uint32_t plane = 0; plane < expectedPlaneCount; ++plane) {
        const BufferPlane &sourcePlane = sourceBuffer->planes.at(plane);
        data[plane].type = dataType;
        data[plane].flags = SPA_DATA_FLAG_READWRITE;
        if (dataType == SPA_DATA_MemFd) {
            data[plane].flags |= SPA_DATA_FLAG_MAPPABLE;
        }
        data[plane].fd = sourcePlane.fd;
        data[plane].mapoffset = 0;
        data[plane].maxsize = sourcePlane.maxSize;
        data[plane].data = nullptr;
        data[plane].chunk->offset = sourcePlane.offset;
        data[plane].chunk->size = sourcePlane.maxSize;
        data[plane].chunk->stride =
                static_cast<int32_t>(sourcePlane.stride);
        data[plane].chunk->flags = SPA_CHUNK_FLAG_NONE;
    }

    buffer->user_data = sourceBuffer;
    m_buffers.append(sourceBuffer);
    qCDebug(SCREENCAST)
                        << (sourceBuffer->transport == BufferTransport::DmaBuf
                                    ? "xdpw: added DMA-BUF buffer"
                                    : "xdpw: added SHM buffer")
                        << buffer
                        << "planes" << sourceBuffer->planeCount
                        << "constraints generation"
                        << sourceBuffer->constraintsGeneration
                        << "buffer generation"
                        << sourceBuffer->bufferGeneration;

    if (m_reconfiguringBuffers
        && sourceBuffer->bufferGeneration == m_bufferGeneration
        && sourceBuffer->constraintsGeneration
                == m_activeConstraints.generation) {
        m_reconfiguringBuffers = false;
        qCInfo(SCREENCAST) << "xdpw: buffer reconfiguration completed at generation"
                           << m_bufferGeneration;
        schedulePendingConstraints();
    }
}

void AbstractPipeWireStream::onStreamRemoveBuffer(pw_buffer *buffer)
{
    if (!buffer || !buffer->buffer) {
        return;
    }

    auto *sourceBuffer = static_cast<PipeWireSourceBuffer *>(buffer->user_data);
    if (m_transaction && m_transaction->pipeWireBuffer == buffer) {
        qCWarning(SCREENCAST) << "xdpw: PipeWire removed in-flight buffer for transaction"
                              << m_transaction->id;
        cancelTransaction("PipeWire removed in-flight buffer");
    }

    for (uint32_t plane = 0;
         buffer->buffer->datas && plane < buffer->buffer->n_datas;
         ++plane) {
        buffer->buffer->datas[plane].fd = -1;
        buffer->buffer->datas[plane].data = nullptr;
    }
    buffer->user_data = nullptr;

    if (sourceBuffer) {
        m_buffers.removeOne(sourceBuffer);
        destroyPipeWireSourceBuffer(sourceBuffer);
    }

    // PipeWire owns the buffer until this callback returns. Defer any pending
    // format update so it cannot synchronously re-enter buffer removal.
    schedulePendingConstraints();
}

void AbstractPipeWireStream::onStreamProcess()
{
    if (m_state != LifecycleState::Streaming
        || m_reconfiguringBuffers
        || m_pendingConstraints
        || m_transaction
        || !m_stream) {
        return;
    }

    pw_buffer *pipeWireBuffer = pw_stream_dequeue_buffer(m_stream);
    if (!pipeWireBuffer) {
        qCDebug(SCREENCAST) << "xdpw: PipeWire process without an available buffer";
        return;
    }

    auto *sourceBuffer = static_cast<PipeWireSourceBuffer *>(pipeWireBuffer->user_data);
    if (!isCurrentBuffer(sourceBuffer)) {
        qCWarning(SCREENCAST) << "xdpw: dequeued stale or invalid capture buffer";
        queueBuffer(pipeWireBuffer,
                    sourceBuffer,
                    false,
                    normalizePresentationTime(monotonicTimeNanoseconds()),
                    WL_OUTPUT_TRANSFORM_NORMAL);
        return;
    }

    FrameTransaction transaction;
    transaction.id = m_nextTransactionId++;
    transaction.pipeWireBuffer = pipeWireBuffer;
    transaction.sourceBuffer = sourceBuffer;
    m_transaction = transaction;
    startFrameCapture();
}

void AbstractPipeWireStream::beginConstraintBatch()
{
    if (m_collectingConstraints) {
        return;
    }

    m_constraintBatch = {};
    m_collectingConstraints = true;
}

bool AbstractPipeWireStream::validateConstraints(const CaptureConstraints &constraints,
                                                 QString *error) const
{
    if (constraints.width == 0 || constraints.height == 0
        || constraints.width > static_cast<uint32_t>(INT_MAX)
        || constraints.height > static_cast<uint32_t>(INT_MAX)) {
        *error = QStringLiteral("invalid capture dimensions %1x%2")
                         .arg(constraints.width)
                         .arg(constraints.height);
        return false;
    }
    const bool hasDmaBufConstraints = constraints.dmaBufDevice
            && !constraints.dmaBufFormats.isEmpty();
    if (constraints.shmFormats.isEmpty() && !hasDmaBufConstraints) {
        *error = QStringLiteral("compositor did not advertise a supported capture format");
        return false;
    }

    for (const ShmFormat &format : constraints.shmFormats) {
        const uint64_t stride = static_cast<uint64_t>(constraints.width)
                * format.bytesPerPixel;
        const uint64_t alignedStride = (stride + 3U) & ~uint64_t(3U);
        const uint64_t size = alignedStride * constraints.height;
        if (alignedStride > static_cast<uint64_t>(INT_MAX)
            || size > static_cast<uint64_t>(INT_MAX)) {
            *error = QStringLiteral("capture buffer is too large");
            return false;
        }
    }

    for (const DmaBufFormat &format : constraints.dmaBufFormats) {
        if (format.drmFormat == DRM_FORMAT_INVALID
            || format.spaFormat == SPA_VIDEO_FORMAT_UNKNOWN
            || format.modifiers.isEmpty()
            || format.modifiers.size() > s_maxDmaBufModifiersPerFormat) {
            *error = QStringLiteral("compositor advertised invalid DMA-BUF constraints");
            return false;
        }
    }

    return true;
}

bool AbstractPipeWireStream::constraintsEqual(const CaptureConstraints &lhs,
                                              const CaptureConstraints &rhs) const
{
    if (lhs.width != rhs.width
        || lhs.height != rhs.height
        || lhs.shmFormats.size() != rhs.shmFormats.size()
        || lhs.dmaBufDevice != rhs.dmaBufDevice
        || lhs.dmaBufFormats.size() != rhs.dmaBufFormats.size()) {
        return false;
    }

    for (qsizetype index = 0; index < lhs.shmFormats.size(); ++index) {
        const ShmFormat &left = lhs.shmFormats.at(index);
        const ShmFormat &right = rhs.shmFormats.at(index);
        if (left.wlFormat != right.wlFormat
            || left.drmFormat != right.drmFormat
            || left.spaFormat != right.spaFormat
            || left.bytesPerPixel != right.bytesPerPixel) {
            return false;
        }
    }

    for (qsizetype index = 0; index < lhs.dmaBufFormats.size(); ++index) {
        const DmaBufFormat &left = lhs.dmaBufFormats.at(index);
        const DmaBufFormat &right = rhs.dmaBufFormats.at(index);
        if (left.drmFormat != right.drmFormat
            || left.spaFormat != right.spaFormat
            || left.modifiers != right.modifiers) {
            return false;
        }
    }
    return true;
}

void AbstractPipeWireStream::applyPendingConstraints()
{
    if (!m_pendingConstraints
        || m_transaction
        || m_reconfiguringBuffers) {
        return;
    }

    const bool constraintsChanged =
            !constraintsEqual(m_activeConstraints, *m_pendingConstraints);
    const bool forceReconfiguration =
            m_pendingConstraintsRequireReconfiguration;
    m_pendingConstraintsRequireReconfiguration = false;

    if (!constraintsChanged) {
        if (forceReconfiguration) {
            qCInfo(SCREENCAST)
                    << "xdpw: re-applying unchanged constraints after buffer rejection"
                    << m_pendingConstraints->generation;
        } else {
            qCDebug(SCREENCAST) << "xdpw: ignoring unchanged constraints generation"
                                << m_pendingConstraints->generation;
        }
        m_pendingConstraints.reset();
        if (forceReconfiguration
            && m_stream
            && !beginBufferReconfiguration(
                    "compositor rejected a buffer against unchanged constraints")) {
            failStream(QStringLiteral("Failed to re-allocate rejected capture buffers"));
        }
        return;
    }

    m_activeConstraints = std::move(*m_pendingConstraints);
    m_pendingConstraints.reset();
    m_negotiatedFormat.reset();
    m_pendingDmaBufSelection.reset();
    refreshDmaBufCapabilities();

    qCInfo(SCREENCAST) << "xdpw: applying constraints generation"
                       << m_activeConstraints.generation
                       << m_activeConstraints.width << "x" << m_activeConstraints.height
                       << "usable DMA-BUF formats" << m_usableDmaBufFormats.size();

    if (m_stream
        && !beginBufferReconfiguration("capture constraints changed")) {
        failStream(QStringLiteral("Failed to renegotiate capture constraints"));
    }
}

void AbstractPipeWireStream::schedulePendingConstraints()
{
    if (isTerminalState()
        || !m_pendingConstraints
        || m_transaction
        || m_reconfiguringBuffers
        || m_constraintsApplyScheduled) {
        return;
    }

    m_constraintsApplyScheduled = true;
    QMetaObject::invokeMethod(this, [this] {
        m_constraintsApplyScheduled = false;
        if (isTerminalState()
            || !m_pendingConstraints
            || m_transaction
            || m_reconfiguringBuffers) {
            return;
        }

        applyPendingConstraints();
        if (!m_stream
            && !isTerminalState()
            && m_activeConstraints.generation != 0
            && !createStream()) {
            failStream(QStringLiteral("Failed to create PipeWire stream"));
        }
    }, Qt::QueuedConnection);
}

void AbstractPipeWireStream::handleCaptureSessionBufferSizeChanged(uint32_t width,
                                                                   uint32_t height)
{
    if (isTerminalState()) {
        return;
    }

    beginConstraintBatch();
    m_constraintBatch.width = width;
    m_constraintBatch.height = height;
}

void AbstractPipeWireStream::handleCaptureSessionShmFormatChanged(uint32_t format)
{
    if (isTerminalState()) {
        return;
    }

    beginConstraintBatch();

    const uint32_t drmFormat =
            PipeWireutils::drmFormatfromWLShmFormat(static_cast<wl_shm_format>(format));
    const int bytesPerPixel = PipeWireutils::pipewireBPPFromDrmFourcc(drmFormat);
    if (bytesPerPixel <= 0) {
        qCInfo(SCREENCAST) << "xdpw: ignoring unsupported compositor SHM format"
                           << Qt::hex << format;
        return;
    }

    const auto existing = std::find_if(m_constraintBatch.shmFormats.cbegin(),
                                       m_constraintBatch.shmFormats.cend(),
                                       [format](const ShmFormat &candidate) {
                                           return candidate.wlFormat == format;
                                       });
    if (existing != m_constraintBatch.shmFormats.cend()) {
        return;
    }
    if (m_constraintBatch.shmFormats.size() >= s_maxShmFormats) {
        qCWarning(SCREENCAST) << "xdpw: ignoring excess compositor SHM format";
        return;
    }

    ShmFormat shmFormat;
    shmFormat.wlFormat = format;
    shmFormat.drmFormat = drmFormat;
    shmFormat.spaFormat = PipeWireutils::pipewireFormatFromDRMFormat(drmFormat);
    if (shmFormat.spaFormat == SPA_VIDEO_FORMAT_UNKNOWN) {
        qCInfo(SCREENCAST) << "xdpw: ignoring unmappable compositor SHM format"
                           << Qt::hex << format << Qt::dec;
        return;
    }
    shmFormat.bytesPerPixel = static_cast<uint32_t>(bytesPerPixel);
    m_constraintBatch.shmFormats.append(shmFormat);
}

void AbstractPipeWireStream::handleCaptureSessionDmaBufDeviceChanged(wl_array *device)
{
    if (isTerminalState()) {
        return;
    }

    beginConstraintBatch();
    if (!device || device->size != sizeof(dev_t) || !device->data) {
        qCWarning(SCREENCAST) << "xdpw: ignoring malformed compositor DMA-BUF device";
        return;
    }

    dev_t deviceId = 0;
    std::memcpy(&deviceId, device->data, sizeof(deviceId));
    m_constraintBatch.dmaBufDevice = deviceId;
}

void AbstractPipeWireStream::handleCaptureSessionDmaBufFormatChanged(
        uint32_t format,
        wl_array *modifiers)
{
    if (isTerminalState()) {
        return;
    }

    beginConstraintBatch();
    if (format == DRM_FORMAT_INVALID
        || !modifiers
        || !modifiers->data
        || modifiers->size == 0
        || modifiers->size % sizeof(uint64_t) != 0) {
        qCWarning(SCREENCAST) << "xdpw: ignoring malformed compositor DMA-BUF format"
                              << Qt::hex << format << Qt::dec;
        return;
    }

    const size_t modifierCount = modifiers->size / sizeof(uint64_t);
    if (modifierCount > static_cast<size_t>(s_maxDmaBufModifiersPerFormat)) {
        qCWarning(SCREENCAST) << "xdpw: ignoring DMA-BUF format with too many modifiers"
                              << modifierCount;
        return;
    }

    const spa_video_format spaFormat =
            PipeWireutils::pipewireFormatFromDRMFormat(format);
    if (spaFormat == SPA_VIDEO_FORMAT_UNKNOWN) {
        qCInfo(SCREENCAST) << "xdpw: ignoring unsupported compositor DMA-BUF format"
                           << Qt::hex << format << Qt::dec;
        return;
    }

    auto existing = std::find_if(m_constraintBatch.dmaBufFormats.begin(),
                                 m_constraintBatch.dmaBufFormats.end(),
                                 [format](const DmaBufFormat &candidate) {
                                     return candidate.drmFormat == format;
                                 });
    if (existing == m_constraintBatch.dmaBufFormats.end()) {
        if (m_constraintBatch.dmaBufFormats.size() >= s_maxDmaBufFormats) {
            qCWarning(SCREENCAST) << "xdpw: ignoring excess compositor DMA-BUF format";
            return;
        }

        DmaBufFormat dmaBufFormat;
        dmaBufFormat.drmFormat = format;
        dmaBufFormat.spaFormat = spaFormat;
        m_constraintBatch.dmaBufFormats.append(std::move(dmaBufFormat));
        existing = std::prev(m_constraintBatch.dmaBufFormats.end());
    }

    const auto *bytes = static_cast<const std::byte *>(modifiers->data);
    for (size_t index = 0; index < modifierCount; ++index) {
        uint64_t modifier = 0;
        std::memcpy(&modifier,
                    bytes + index * sizeof(modifier),
                    sizeof(modifier));
        if (!existing->modifiers.contains(modifier)) {
            if (existing->modifiers.size() >= s_maxDmaBufModifiersPerFormat) {
                qCWarning(SCREENCAST)
                        << "xdpw: truncating excess compositor DMA-BUF modifiers";
                break;
            }
            existing->modifiers.append(modifier);
        }
    }
}

void AbstractPipeWireStream::handleCaptureSessionDone()
{
    if (isTerminalState()) {
        return;
    }

    if (!m_collectingConstraints) {
        failStream(QStringLiteral("Compositor sent an empty constraints batch"));
        return;
    }

    m_collectingConstraints = false;
    m_constraintBatch.generation = m_nextConstraintGeneration++;

    QString error;
    if (!validateConstraints(m_constraintBatch, &error)) {
        failStream(QStringLiteral("Invalid image-copy constraints: %1").arg(error));
        return;
    }

    qCInfo(SCREENCAST) << "xdpw: received constraints generation"
                       << m_constraintBatch.generation
                       << m_constraintBatch.width << "x" << m_constraintBatch.height
                       << "SHM formats" << m_constraintBatch.shmFormats.size()
                       << "DMA-BUF formats" << m_constraintBatch.dmaBufFormats.size();

    m_pendingConstraints = std::move(m_constraintBatch);
    if (m_transaction) {
        qCInfo(SCREENCAST) << "xdpw: deferring constraints until transaction"
                           << m_transaction->id << "finishes";
    } else if (m_reconfiguringBuffers) {
        qCInfo(SCREENCAST) << "xdpw: coalescing constraints while buffer generation"
                           << m_bufferGeneration << "is being configured";
    }

    // A queued hand-off coalesces complete constraint batches dispatched in
    // the same event-loop turn. Correctness does not depend on a timeout: an
    // active frame transaction or PipeWire buffer reconfiguration remains the
    // hard serialization boundary.
    schedulePendingConstraints();
}

void AbstractPipeWireStream::handleCaptureSessionStopped()
{
    if (isTerminalState()) {
        return;
    }

    qCWarning(SCREENCAST) << "xdpw: image-copy session stopped";
    failStream(QStringLiteral("Image-copy capture session stopped"));
}

void AbstractPipeWireStream::startFrameCapture()
{
    if (!m_transaction || !m_session || m_state != LifecycleState::Streaming) {
        if (m_transaction) {
            finishTransaction(false, "capture cannot start in current state");
        }
        return;
    }

    ext_image_copy_capture_frame_v1 *frameObject = m_session->create_frame();
    if (!frameObject) {
        finishTransaction(false, "compositor did not create a frame object");
        return;
    }

    auto *frame = new ImageCopyCaptureFrame(frameObject, this);
    m_transaction->captureFrame = frame;
    connect(frame, &ImageCopyCaptureFrame::transformChanged,
            this, &AbstractPipeWireStream::handleFrameTransform);
    connect(frame, &ImageCopyCaptureFrame::presentationTimeChanged,
            this, &AbstractPipeWireStream::handleFramePresentationTime);
    connect(frame, &ImageCopyCaptureFrame::ready,
            this, &AbstractPipeWireStream::handleFrameReady);
    connect(frame, &ImageCopyCaptureFrame::failed,
            this, &AbstractPipeWireStream::handleFrameFailed);

    frame->attach_buffer(m_transaction->sourceBuffer->waylandBuffer);
    frame->damage_buffer(0,
                         0,
                         static_cast<int32_t>(m_transaction->sourceBuffer->width),
                         static_cast<int32_t>(m_transaction->sourceBuffer->height));
    frame->capture();

    qCDebug(SCREENCAST) << "xdpw: transaction" << m_transaction->id
                        << "capture requested for PipeWire buffer"
                        << m_transaction->pipeWireBuffer;
}

void AbstractPipeWireStream::handleFrameTransform(uint32_t transform)
{
    if (!m_transaction) {
        return;
    }
    if (transform > WL_OUTPUT_TRANSFORM_FLIPPED_270) {
        qCWarning(SCREENCAST) << "xdpw: transaction" << m_transaction->id
                              << "received invalid transform" << transform;
        return;
    }
    m_transaction->transform = transform;
    m_transaction->transformReceived = true;
}

void AbstractPipeWireStream::handleFramePresentationTime(uint32_t tvSecHi,
                                                         uint32_t tvSecLo,
                                                         uint32_t tvNsec)
{
    if (!m_transaction) {
        return;
    }

    m_transaction->presentationSeconds =
            (static_cast<uint64_t>(tvSecHi) << 32) | tvSecLo;
    m_transaction->presentationNanoseconds = tvNsec;
    m_transaction->timestampReceived = tvNsec < s_nanosecondsPerSecond;
}

void AbstractPipeWireStream::handleFrameReady()
{
    if (!m_transaction) {
        qCWarning(SCREENCAST) << "xdpw: received ready without an active transaction";
        return;
    }

    const bool validMetadata = m_transaction->timestampReceived
            && m_transaction->transformReceived;
    finishTransaction(validMetadata,
                      validMetadata ? "frame ready" : "frame ready with incomplete metadata");
}

void AbstractPipeWireStream::handleFrameFailed(uint32_t reason)
{
    if (!m_transaction) {
        qCWarning(SCREENCAST) << "xdpw: received failed without an active transaction";
        return;
    }

    const bool bufferConstraintsChanged =
            reason == EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_BUFFER_CONSTRAINTS;
    const QByteArray reasonText = frameFailureReason(reason).toUtf8();
    finishTransaction(false, reasonText.constData());

    // The protocol requires the client to re-allocate its buffers against the
    // latest constraints. It does not require the compositor to send a fresh
    // constraints batch after reporting this failure.
    if (!bufferConstraintsChanged
        || isTerminalState()
        || m_reconfiguringBuffers) {
        return;
    }

    if (m_pendingConstraints) {
        // When a complete newer constraints batch is already pending, the
        // queued latest-wins path will reconfigure against it. This flag is
        // required even if that batch is byte-for-byte equal: BUFFER_CONSTRAINTS
        // explicitly tells the client to re-allocate. Treating the old buffer's
        // modifier as rejected here would mutate the capabilities of the wrong
        // constraints generation.
        m_pendingConstraintsRequireReconfiguration = true;
        return;
    }

    if (m_negotiatedFormat
        && m_negotiatedFormat->transport == BufferTransport::DmaBuf) {
        const QVector<uint64_t> rejectedModifiers{
            m_negotiatedFormat->modifier,
        };
        if (rejectDmaBufModifiers(m_negotiatedFormat->spaFormat,
                                  rejectedModifiers)) {
            qCWarning(SCREENCAST)
                    << "xdpw: compositor rejected DMA-BUF modifier"
                    << Qt::hex << m_negotiatedFormat->modifier << Qt::dec;
        }
    }

    if (!beginBufferReconfiguration("compositor rejected capture buffer")) {
        failStream(QStringLiteral("Failed to re-allocate rejected capture buffers"));
    }
}

void AbstractPipeWireStream::destroyCaptureFrame()
{
    if (!m_transaction || !m_transaction->captureFrame) {
        return;
    }

    ImageCopyCaptureFrame *frame = m_transaction->captureFrame;
    m_transaction->captureFrame = nullptr;
    QObject::disconnect(frame, nullptr, this, nullptr);
    frame->destroy();
    frame->deleteLater();
}

void AbstractPipeWireStream::cancelTransaction(const char *reason)
{
    if (!m_transaction) {
        return;
    }

    qCInfo(SCREENCAST) << "xdpw: cancelling transaction" << m_transaction->id
                       << reason;
    destroyCaptureFrame();
    m_transaction.reset();
}

void AbstractPipeWireStream::finishTransaction(bool validFrame, const char *reason)
{
    if (!m_transaction) {
        return;
    }

    const uint64_t transactionId = m_transaction->id;
    pw_buffer *pipeWireBuffer = m_transaction->pipeWireBuffer;
    PipeWireSourceBuffer *sourceBuffer = m_transaction->sourceBuffer;
    uint32_t transform = m_transaction->transform;

    bool validTimestamp = false;
    uint64_t pts = transactionPresentationTime(&validTimestamp);
    validFrame = validFrame && validTimestamp;
    if (!validTimestamp) {
        pts = monotonicTimeNanoseconds();
        transform = WL_OUTPUT_TRANSFORM_NORMAL;
    }
    pts = normalizePresentationTime(pts);

    destroyCaptureFrame();
    m_transaction.reset();

    qCDebug(SCREENCAST) << "xdpw: finishing transaction" << transactionId
                        << (validFrame ? "valid" : "corrupted")
                        << reason << "pts" << pts;
    queueBuffer(pipeWireBuffer, sourceBuffer, validFrame, pts, transform);
    schedulePendingConstraints();
}

void AbstractPipeWireStream::refreshDmaBufCapabilities()
{
    m_usableDmaBufFormats.clear();

    if (!m_activeConstraints.dmaBufDevice
        || m_activeConstraints.dmaBufFormats.isEmpty()
        || !m_context
        || !m_context->m_linuxDmaBuf
        || !m_context->linuxDmaBufInterfaceActive()) {
        m_dmaBufDevice.reset();
        return;
    }

    if (!m_dmaBufDevice
        || m_dmaBufDevice->deviceId != *m_activeConstraints.dmaBufDevice) {
        m_dmaBufDevice = createDmaBufDevice(*m_activeConstraints.dmaBufDevice);
    }
    if (!m_dmaBufDevice) {
        qCWarning(SCREENCAST)
                << "xdpw: compositor advertised DMA-BUF but its DRM device is unavailable";
        return;
    }

    for (const DmaBufFormat &format : std::as_const(m_activeConstraints.dmaBufFormats)) {
        DmaBufFormat usableFormat;
        usableFormat.drmFormat = format.drmFormat;
        usableFormat.spaFormat = format.spaFormat;

        for (uint64_t modifier : format.modifiers) {
            if (modifier == DRM_FORMAT_MOD_INVALID
                || modifier == DRM_FORMAT_MOD_LINEAR
                || gbm_device_get_format_modifier_plane_count(
                           m_dmaBufDevice->gbm,
                           format.drmFormat,
                           modifier) > 0) {
                usableFormat.modifiers.append(modifier);
            }
        }

        // Avoid publishing a DMA-BUF format when even its best currently
        // advertised modifier cannot be allocated. Only one successful probe
        // is needed here; the exact consumer-selected modifier is tested again
        // during fixation.
        bool allocationAvailable = false;
        for (uint64_t modifier : std::as_const(usableFormat.modifiers)) {
            gbm_bo *testBo = allocateDmaBufBo(m_dmaBufDevice,
                                              m_activeConstraints.width,
                                              m_activeConstraints.height,
                                              format.drmFormat,
                                              modifier);
            if (testBo) {
                gbm_bo_destroy(testBo);
                allocationAvailable = true;
                break;
            }
        }

        if (allocationAvailable) {
            m_usableDmaBufFormats.append(std::move(usableFormat));
        }
    }
}

std::shared_ptr<AbstractPipeWireStream::DmaBufDevice>
AbstractPipeWireStream::createDmaBufDevice(dev_t deviceId) const
{
    drmDevice *drmDeviceInfo = nullptr;
    const int resolveResult =
            drmGetDeviceFromDevId(deviceId, 0, &drmDeviceInfo);
    if (resolveResult != 0 || !drmDeviceInfo) {
        if (drmDeviceInfo) {
            drmFreeDevice(&drmDeviceInfo);
        }
        qCWarning(SCREENCAST) << "xdpw: failed to resolve compositor DRM device";
        return {};
    }

    const char *node = nullptr;
    if ((drmDeviceInfo->available_nodes & (1 << DRM_NODE_RENDER)) != 0) {
        node = drmDeviceInfo->nodes[DRM_NODE_RENDER];
    } else if ((drmDeviceInfo->available_nodes & (1 << DRM_NODE_PRIMARY)) != 0) {
        node = drmDeviceInfo->nodes[DRM_NODE_PRIMARY];
    }

    if (!node) {
        qCWarning(SCREENCAST) << "xdpw: compositor DRM device has no usable node";
        drmFreeDevice(&drmDeviceInfo);
        return {};
    }

    const QByteArray nodePath(node);
    drmFreeDevice(&drmDeviceInfo);

    const int fd = open(nodePath.constData(), O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        qCWarning(SCREENCAST) << "xdpw: failed to open compositor DRM node"
                              << nodePath;
        return {};
    }

    gbm_device *gbm = gbm_create_device(fd);
    if (!gbm) {
        qCWarning(SCREENCAST) << "xdpw: failed to create GBM device for"
                              << nodePath;
        close(fd);
        return {};
    }

    auto device = std::make_shared<DmaBufDevice>();
    device->deviceId = deviceId;
    device->fd = fd;
    device->gbm = gbm;
    qCInfo(SCREENCAST) << "xdpw: DMA-BUF device ready" << nodePath;
    return device;
}

gbm_bo *AbstractPipeWireStream::allocateDmaBufBo(
        const std::shared_ptr<DmaBufDevice> &device,
        uint32_t width,
        uint32_t height,
        uint32_t drmFormat,
        uint64_t modifier) const
{
    if (!device || !device->gbm || width == 0 || height == 0
        || drmFormat == DRM_FORMAT_INVALID) {
        return nullptr;
    }

    constexpr uint32_t renderingFlags = GBM_BO_USE_RENDERING;
    gbm_bo *bo = nullptr;
    bool linearLegacyAllocation = false;
    if (modifier == DRM_FORMAT_MOD_INVALID) {
        bo = gbm_bo_create(device->gbm,
                           width,
                           height,
                           drmFormat,
                           renderingFlags);
    } else {
        bo = gbm_bo_create_with_modifiers2(device->gbm,
                                           width,
                                           height,
                                           drmFormat,
                                           &modifier,
                                           1,
                                           renderingFlags);
        if (!bo && modifier == DRM_FORMAT_MOD_LINEAR) {
            bo = gbm_bo_create(device->gbm,
                               width,
                               height,
                               drmFormat,
                               renderingFlags | GBM_BO_USE_LINEAR);
            linearLegacyAllocation = bo != nullptr;
        }
    }

    if (!bo) {
        return nullptr;
    }

    const int planeCount = gbm_bo_get_plane_count(bo);
    const uint64_t actualModifier = gbm_bo_get_modifier(bo);
    const bool geometryMatches = gbm_bo_get_width(bo) == width
            && gbm_bo_get_height(bo) == height
            && gbm_bo_get_format(bo) == drmFormat;
    const bool modifierMatches = modifier == DRM_FORMAT_MOD_INVALID
            || actualModifier == modifier
            || (modifier == DRM_FORMAT_MOD_LINEAR && linearLegacyAllocation);
    if (!geometryMatches || !modifierMatches
        || planeCount <= 0 || planeCount > GBM_MAX_PLANES) {
        gbm_bo_destroy(bo);
        return nullptr;
    }
    return bo;
}

std::optional<AbstractPipeWireStream::NegotiatedFormat>
AbstractPipeWireStream::selectDmaBufFormat(
        spa_video_format spaFormat,
        const QVector<uint64_t> &modifiers) const
{
    if (!m_dmaBufDevice || modifiers.isEmpty()) {
        return std::nullopt;
    }

    const auto tryModifier = [this, spaFormat](
                                     const DmaBufFormat &format,
                                     uint64_t modifier)
            -> std::optional<NegotiatedFormat> {
        if (format.spaFormat != spaFormat
            || !format.modifiers.contains(modifier)) {
            return std::nullopt;
        }

        gbm_bo *testBo = allocateDmaBufBo(m_dmaBufDevice,
                                          m_activeConstraints.width,
                                          m_activeConstraints.height,
                                          format.drmFormat,
                                          modifier);
        if (!testBo) {
            return std::nullopt;
        }
        const int planeCount = gbm_bo_get_plane_count(testBo);
        gbm_bo_destroy(testBo);
        if (planeCount <= 0 || planeCount > GBM_MAX_PLANES) {
            return std::nullopt;
        }

        NegotiatedFormat result;
        result.transport = BufferTransport::DmaBuf;
        result.drmFormat = format.drmFormat;
        result.spaFormat = format.spaFormat;
        result.width = m_activeConstraints.width;
        result.height = m_activeConstraints.height;
        result.modifier = modifier;
        result.planeCount = static_cast<uint32_t>(planeCount);
        result.constraintsGeneration = m_activeConstraints.generation;
        result.dmaBufDevice = m_dmaBufDevice;
        return result;
    };

    // Explicit modifiers are preferable. DRM_FORMAT_MOD_INVALID is an
    // implicit-modifier compatibility fallback and is tried last.
    for (bool implicitPass : {false, true}) {
        for (const DmaBufFormat &format : m_usableDmaBufFormats) {
            for (uint64_t modifier : modifiers) {
                if ((modifier == DRM_FORMAT_MOD_INVALID) != implicitPass) {
                    continue;
                }
                if (std::optional<NegotiatedFormat> result =
                            tryModifier(format, modifier)) {
                    return result;
                }
            }
        }
    }
    return std::nullopt;
}

bool AbstractPipeWireStream::rejectDmaBufModifiers(
        spa_video_format spaFormat,
        const QVector<uint64_t> &modifiers)
{
    bool removed = false;
    for (DmaBufFormat &format : m_usableDmaBufFormats) {
        if (format.spaFormat != spaFormat) {
            continue;
        }

        const auto newEnd =
                std::remove_if(format.modifiers.begin(),
                               format.modifiers.end(),
                               [&modifiers](uint64_t modifier) {
                                   return modifiers.contains(modifier);
                               });
        removed = removed || newEnd != format.modifiers.end();
        format.modifiers.erase(newEnd, format.modifiers.end());
    }

    m_usableDmaBufFormats.erase(
            std::remove_if(m_usableDmaBufFormats.begin(),
                           m_usableDmaBufFormats.end(),
                           [](const DmaBufFormat &format) {
                               return format.modifiers.isEmpty();
                           }),
            m_usableDmaBufFormats.end());
    return removed;
}

QVector<QByteArray> AbstractPipeWireStream::buildStreamFormatPods() const
{
    QVector<QByteArray> result;
    result.reserve(m_usableDmaBufFormats.size()
                   + m_activeConstraints.shmFormats.size() * 2
                   + (m_pendingDmaBufSelection ? 1 : 0));

    const QVector<uint64_t> noModifiers;
    const auto appendPod = [&result](std::optional<QByteArray> pod) {
        if (pod) {
            result.append(std::move(*pod));
            return true;
        }
        return false;
    };

    if (m_pendingDmaBufSelection
        && m_pendingDmaBufSelection->width == m_activeConstraints.width
        && m_pendingDmaBufSelection->height == m_activeConstraints.height
        && m_pendingDmaBufSelection->constraintsGeneration
                == m_activeConstraints.generation
        && !appendPod(buildFormatPod(m_pendingDmaBufSelection->spaFormat,
                                     m_activeConstraints.width,
                                     m_activeConstraints.height,
                                     m_maxFramerate,
                                     noModifiers,
                                     m_pendingDmaBufSelection->modifier))) {
        return {};
    }

    for (const DmaBufFormat &format : m_usableDmaBufFormats) {
        if (!appendPod(buildFormatPod(format.spaFormat,
                                      m_activeConstraints.width,
                                      m_activeConstraints.height,
                                      m_maxFramerate,
                                      format.modifiers))) {
            return {};
        }
    }
    QVector<spa_video_format> advertisedShmFormats;
    advertisedShmFormats.reserve(m_activeConstraints.shmFormats.size() * 2);
    for (const ShmFormat &format : m_activeConstraints.shmFormats) {
        for (spa_video_format spaFormat :
             compatibleShmSpaFormats(format.spaFormat)) {
            if (advertisedShmFormats.contains(spaFormat)) {
                continue;
            }
            if (!appendPod(buildFormatPod(spaFormat,
                                          m_activeConstraints.width,
                                          m_activeConstraints.height,
                                          m_maxFramerate,
                                          noModifiers))) {
                return {};
            }
            advertisedShmFormats.append(spaFormat);
        }
    }
    return result;
}

bool AbstractPipeWireStream::createStream()
{
    if (m_stream || !m_pipeWireCore || !m_pipeWireCore->isValid()) {
        return false;
    }

    m_stream = pw_stream_new(m_pipeWireCore->m_pwCore,
                             "xdpw-dde-screencast",
                             pw_properties_new(PW_KEY_MEDIA_CLASS,
                                               "Video/Source",
                                               nullptr));
    if (!m_stream) {
        return false;
    }

    pw_stream_add_listener(m_stream, &m_streamListener, &s_streamEvents, this);

    const QVector<QByteArray> formatPods = buildStreamFormatPods();
    QVarLengthArray<const spa_pod *, 16> params;
    params.reserve(formatPods.size());
    for (const QByteArray &pod : formatPods) {
        params.append(reinterpret_cast<const spa_pod *>(pod.constData()));
    }
    if (params.isEmpty()) {
        destroyStream();
        return false;
    }

    const int result = pw_stream_connect(m_stream,
                                         PW_DIRECTION_OUTPUT,
                                         PW_ID_ANY,
                                         PW_STREAM_FLAG_ALLOC_BUFFERS,
                                         params.data(),
                                         static_cast<uint32_t>(params.size()));
    if (result < 0) {
        qCWarning(SCREENCAST) << "xdpw: pw_stream_connect failed"
                              << spa_strerror(result);
        destroyStream();
        return false;
    }

    qCInfo(SCREENCAST) << "xdpw: PipeWire stream connecting with"
                       << m_usableDmaBufFormats.size() << "DMA-BUF formats and"
                       << m_activeConstraints.shmFormats.size()
                       << "SHM formats; max framerate"
                       << m_maxFramerate;
    return true;
}

void AbstractPipeWireStream::destroyStream()
{
    if (!m_stream) {
        return;
    }

    pw_stream *stream = m_stream;
    m_stream = nullptr;
    pw_stream_destroy(stream);
    m_streamListener = {};
}

bool AbstractPipeWireStream::updateStreamFormats()
{
    if (!m_stream) {
        return false;
    }

    const QVector<QByteArray> formatPods = buildStreamFormatPods();
    QVarLengthArray<const spa_pod *, 16> params;
    params.reserve(formatPods.size());
    for (const QByteArray &pod : formatPods) {
        params.append(reinterpret_cast<const spa_pod *>(pod.constData()));
    }

    return !params.isEmpty()
            && pw_stream_update_params(m_stream,
                                       params.data(),
                                       static_cast<uint32_t>(params.size())) >= 0;
}

bool AbstractPipeWireStream::isCurrentNegotiatedFormat() const
{
    return m_negotiatedFormat
            && m_negotiatedFormat->width == m_activeConstraints.width
            && m_negotiatedFormat->height == m_activeConstraints.height
            && m_negotiatedFormat->constraintsGeneration
                    == m_activeConstraints.generation;
}

bool AbstractPipeWireStream::updateStreamBufferParams()
{
    if (!m_stream || !isCurrentNegotiatedFormat()) {
        return false;
    }

    uint32_t blocks = m_negotiatedFormat->planeCount;
    uint32_t dataType = 1U << SPA_DATA_DmaBuf;
    uint32_t size = 0;
    uint32_t stride = 0;
    if (m_negotiatedFormat->transport == BufferTransport::Shm) {
        const uint64_t unalignedStride =
                static_cast<uint64_t>(m_negotiatedFormat->width)
                * m_negotiatedFormat->bytesPerPixel;
        const uint64_t alignedStride =
                (unalignedStride + 3U) & ~uint64_t(3U);
        const uint64_t bufferSize =
                alignedStride * m_negotiatedFormat->height;
        if (alignedStride > static_cast<uint64_t>(INT_MAX)
            || bufferSize > static_cast<uint64_t>(INT_MAX)) {
            return false;
        }
        blocks = 1;
        dataType = 1U << SPA_DATA_MemFd;
        stride = static_cast<uint32_t>(alignedStride);
        size = static_cast<uint32_t>(bufferSize);
    } else if (blocks == 0 || blocks > GBM_MAX_PLANES) {
        return false;
    }

    uint8_t podStorage[2048];
    spa_pod_builder builder = SPA_POD_BUILDER_INIT(podStorage, sizeof(podStorage));
    QVarLengthArray<const spa_pod *, 4> params;
    params.append(buildBufferParam(&builder,
                                   blocks,
                                   dataType,
                                   size,
                                   stride));
    params.append(static_cast<spa_pod *>(
            spa_pod_builder_add_object(&builder,
                                       SPA_TYPE_OBJECT_ParamMeta,
                                       SPA_PARAM_Meta,
                                       SPA_PARAM_META_type,
                                       SPA_POD_Id(SPA_META_Header),
                                       SPA_PARAM_META_size,
                                       SPA_POD_Int(sizeof(spa_meta_header)))));
    params.append(static_cast<spa_pod *>(
            spa_pod_builder_add_object(&builder,
                                       SPA_TYPE_OBJECT_ParamMeta,
                                       SPA_PARAM_Meta,
                                       SPA_PARAM_META_type,
                                       SPA_POD_Id(SPA_META_VideoTransform),
                                       SPA_PARAM_META_size,
                                       SPA_POD_Int(sizeof(spa_meta_videotransform)))));

    return pw_stream_update_params(m_stream,
                                   params.data(),
                                   static_cast<uint32_t>(params.size())) >= 0;
}

bool AbstractPipeWireStream::beginBufferReconfiguration(const char *reason)
{
    if (!m_stream) {
        return false;
    }
    if (m_reconfiguringBuffers) {
        return true;
    }
    if (m_bufferGeneration == std::numeric_limits<uint64_t>::max()) {
        return false;
    }

    ++m_bufferGeneration;
    m_reconfiguringBuffers = true;
    m_negotiatedFormat.reset();
    m_pendingDmaBufSelection.reset();

    qCInfo(SCREENCAST) << "xdpw: beginning buffer reconfiguration"
                       << reason
                       << "constraints generation" << m_activeConstraints.generation
                       << "buffer generation" << m_bufferGeneration;

    if (!updateStreamFormats()) {
        m_reconfiguringBuffers = false;
        return false;
    }
    return true;
}

AbstractPipeWireStream::PipeWireSourceBuffer *
AbstractPipeWireStream::createPipeWireSourceBuffer() const
{
    if (!m_context || !isCurrentNegotiatedFormat()) {
        return nullptr;
    }
    return m_negotiatedFormat->transport == BufferTransport::DmaBuf
            ? createDmaBufPipeWireSourceBuffer()
            : createShmPipeWireSourceBuffer();
}

AbstractPipeWireStream::PipeWireSourceBuffer *
AbstractPipeWireStream::createShmPipeWireSourceBuffer() const
{
    if (!m_context || !isCurrentNegotiatedFormat()
        || m_negotiatedFormat->transport != BufferTransport::Shm) {
        return nullptr;
    }

    const uint64_t unalignedStride =
            static_cast<uint64_t>(m_negotiatedFormat->width)
            * m_negotiatedFormat->bytesPerPixel;
    const uint64_t stride = (unalignedStride + 3U) & ~uint64_t(3U);
    const uint64_t size = stride * m_negotiatedFormat->height;
    if (stride > static_cast<uint64_t>(INT_MAX)
        || size > static_cast<uint64_t>(INT_MAX)) {
        return nullptr;
    }

    auto *buffer = new PipeWireSourceBuffer;
    buffer->transport = BufferTransport::Shm;
    buffer->planeCount = 1;
    buffer->width = m_negotiatedFormat->width;
    buffer->height = m_negotiatedFormat->height;
    buffer->wlFormat = m_negotiatedFormat->wlFormat;
    buffer->drmFormat = m_negotiatedFormat->drmFormat;
    buffer->constraintsGeneration =
            m_negotiatedFormat->constraintsGeneration;
    buffer->bufferGeneration = m_bufferGeneration;
    BufferPlane &plane = buffer->planes[0];
    plane.stride = static_cast<uint32_t>(stride);
    plane.maxSize = static_cast<uint32_t>(size);
    plane.fd =
            ScreenCastMemory::createSealedMemFd("xdpw-dde-screencast",
                                                plane.maxSize);
    if (plane.fd < 0) {
        destroyPipeWireSourceBuffer(buffer);
        return nullptr;
    }

    buffer->waylandBuffer =
            m_context->createWLSHMBuffer(plane.fd,
                                         static_cast<wl_shm_format>(buffer->wlFormat),
                                         static_cast<int>(buffer->width),
                                         static_cast<int>(buffer->height),
                                         static_cast<int>(plane.stride));
    if (!buffer->waylandBuffer) {
        destroyPipeWireSourceBuffer(buffer);
        return nullptr;
    }

    return buffer;
}

AbstractPipeWireStream::PipeWireSourceBuffer *
AbstractPipeWireStream::createDmaBufPipeWireSourceBuffer() const
{
    if (!m_context || !m_context->m_linuxDmaBuf
        || !m_context->linuxDmaBufInterfaceActive()
        || !isCurrentNegotiatedFormat()
        || m_negotiatedFormat->transport != BufferTransport::DmaBuf
        || !m_negotiatedFormat->dmaBufDevice) {
        return nullptr;
    }

    auto *buffer = new PipeWireSourceBuffer;
    buffer->transport = BufferTransport::DmaBuf;
    buffer->width = m_negotiatedFormat->width;
    buffer->height = m_negotiatedFormat->height;
    buffer->drmFormat = m_negotiatedFormat->drmFormat;
    buffer->modifier = m_negotiatedFormat->modifier;
    buffer->constraintsGeneration =
            m_negotiatedFormat->constraintsGeneration;
    buffer->bufferGeneration = m_bufferGeneration;
    buffer->dmaBufDevice = m_negotiatedFormat->dmaBufDevice;
    buffer->gbmBo = allocateDmaBufBo(buffer->dmaBufDevice,
                                     buffer->width,
                                     buffer->height,
                                     buffer->drmFormat,
                                     buffer->modifier);
    if (!buffer->gbmBo) {
        destroyPipeWireSourceBuffer(buffer);
        return nullptr;
    }

    const int planeCount = gbm_bo_get_plane_count(buffer->gbmBo);
    if (planeCount <= 0
        || planeCount > GBM_MAX_PLANES
        || static_cast<uint32_t>(planeCount) != m_negotiatedFormat->planeCount) {
        destroyPipeWireSourceBuffer(buffer);
        return nullptr;
    }
    buffer->planeCount = static_cast<uint32_t>(planeCount);

    for (uint32_t planeIndex = 0;
         planeIndex < buffer->planeCount;
         ++planeIndex) {
        BufferPlane &plane = buffer->planes.at(planeIndex);
        plane.fd = gbm_bo_get_fd_for_plane(buffer->gbmBo,
                                           static_cast<int>(planeIndex));
        if (plane.fd < 0 && buffer->planeCount == 1 && planeIndex == 0) {
            plane.fd = gbm_bo_get_fd(buffer->gbmBo);
        }
        plane.stride = gbm_bo_get_stride_for_plane(
                buffer->gbmBo, static_cast<int>(planeIndex));
        plane.offset = gbm_bo_get_offset(buffer->gbmBo,
                                         static_cast<int>(planeIndex));
        if (plane.fd < 0
            || plane.stride == 0
            || plane.stride > static_cast<uint32_t>(INT_MAX)) {
            destroyPipeWireSourceBuffer(buffer);
            return nullptr;
        }

        // DMA-BUF allocation size is not generally queryable. PipeWire and
        // consumers only require the first plane to carry non-zero video
        // content; pitch * image height is the conservative convention also
        // used by KWin. Per-plane offsets and pitches remain exact.
        if (planeIndex == 0) {
            const uint64_t maximumSize =
                    static_cast<uint64_t>(plane.stride) * buffer->height;
            if (maximumSize == 0
                || maximumSize > std::numeric_limits<uint32_t>::max()) {
                destroyPipeWireSourceBuffer(buffer);
                return nullptr;
            }
            plane.maxSize = static_cast<uint32_t>(maximumSize);
        }
    }

    zwp_linux_buffer_params_v1 *params =
            m_context->m_linuxDmaBuf->create_params();
    if (!params) {
        destroyPipeWireSourceBuffer(buffer);
        return nullptr;
    }

    for (uint32_t planeIndex = 0;
         planeIndex < buffer->planeCount;
         ++planeIndex) {
        const BufferPlane &plane = buffer->planes.at(planeIndex);
        zwp_linux_buffer_params_v1_add(params,
                                       plane.fd,
                                       planeIndex,
                                       plane.offset,
                                       plane.stride,
                                       static_cast<uint32_t>(buffer->modifier >> 32),
                                       static_cast<uint32_t>(buffer->modifier));
    }
    buffer->waylandBuffer =
            zwp_linux_buffer_params_v1_create_immed(
                    params,
                    static_cast<int32_t>(buffer->width),
                    static_cast<int32_t>(buffer->height),
                    buffer->drmFormat,
                    0);
    zwp_linux_buffer_params_v1_destroy(params);
    if (!buffer->waylandBuffer) {
        destroyPipeWireSourceBuffer(buffer);
        return nullptr;
    }

    return buffer;
}

void AbstractPipeWireStream::destroyPipeWireSourceBuffer(PipeWireSourceBuffer *buffer) const
{
    if (!buffer) {
        return;
    }
    if (buffer->waylandBuffer) {
        wl_buffer_destroy(buffer->waylandBuffer);
        buffer->waylandBuffer = nullptr;
    }
    for (BufferPlane &plane : buffer->planes) {
        if (plane.fd >= 0) {
            close(plane.fd);
            plane.fd = -1;
        }
    }
    if (buffer->gbmBo) {
        gbm_bo_destroy(buffer->gbmBo);
        buffer->gbmBo = nullptr;
    }
    buffer->dmaBufDevice.reset();
    delete buffer;
}

bool AbstractPipeWireStream::isCurrentBuffer(const PipeWireSourceBuffer *buffer) const
{
    return buffer
            && buffer->waylandBuffer
            && isCurrentNegotiatedFormat()
            && buffer->bufferGeneration == m_bufferGeneration
            && buffer->constraintsGeneration
                    == m_negotiatedFormat->constraintsGeneration
            && buffer->width == m_negotiatedFormat->width
            && buffer->height == m_negotiatedFormat->height
            && buffer->transport == m_negotiatedFormat->transport
            && buffer->drmFormat == m_negotiatedFormat->drmFormat
            && buffer->modifier == m_negotiatedFormat->modifier
            && buffer->planeCount == m_negotiatedFormat->planeCount
            && (buffer->transport != BufferTransport::Shm
                || buffer->wlFormat == m_negotiatedFormat->wlFormat)
            && (buffer->transport != BufferTransport::DmaBuf
                || buffer->dmaBufDevice == m_negotiatedFormat->dmaBufDevice);
}

void AbstractPipeWireStream::queueBuffer(pw_buffer *pipeWireBuffer,
                                         PipeWireSourceBuffer *sourceBuffer,
                                         bool validFrame,
                                         uint64_t pts,
                                         uint32_t transform)
{
    if (!pipeWireBuffer || !pipeWireBuffer->buffer || !m_stream) {
        return;
    }

    spa_buffer *spaBuffer = pipeWireBuffer->buffer;
    bool validLayout = sourceBuffer
            && sourceBuffer->planeCount > 0
            && sourceBuffer->planeCount <= GBM_MAX_PLANES
            && spaBuffer->n_datas == sourceBuffer->planeCount
            && spaBuffer->datas;
    for (uint32_t plane = 0;
         validLayout && plane < spaBuffer->n_datas;
         ++plane) {
        validLayout = spaBuffer->datas[plane].chunk != nullptr;
    }
    validFrame = validFrame && sourceBuffer && validLayout;
    if (!validLayout) {
        qCWarning(SCREENCAST) << "xdpw: queuing a malformed PipeWire buffer as corrupted";
    }

    if (auto *header = static_cast<spa_meta_header *>(
                spa_buffer_find_meta_data(spaBuffer, SPA_META_Header, sizeof(spa_meta_header)))) {
        header->pts = static_cast<int64_t>(pts);
        header->flags = validFrame ? 0 : SPA_META_HEADER_FLAG_CORRUPTED;
        header->seq = m_sequence++;
        header->dts_offset = 0;
    }

    if (auto *videoTransform = static_cast<spa_meta_videotransform *>(
                spa_buffer_find_meta_data(spaBuffer,
                                          SPA_META_VideoTransform,
                                          sizeof(spa_meta_videotransform)))) {
        videoTransform->transform = transform;
    }

    for (uint32_t plane = 0; spaBuffer->datas && plane < spaBuffer->n_datas; ++plane) {
        spa_data &data = spaBuffer->datas[plane];
        if (!data.chunk) {
            continue;
        }
        const BufferPlane *sourcePlane =
                sourceBuffer && plane < sourceBuffer->planeCount
                ? &sourceBuffer->planes.at(plane)
                : nullptr;
        data.chunk->offset = sourcePlane ? sourcePlane->offset : 0;
        data.chunk->size = validFrame && sourcePlane ? sourcePlane->maxSize : 0;
        data.chunk->stride = sourcePlane
                ? static_cast<int32_t>(sourcePlane->stride)
                : 0;
        data.chunk->flags = validFrame
                ? SPA_CHUNK_FLAG_NONE
                : SPA_CHUNK_FLAG_CORRUPTED;
    }

    pw_stream_queue_buffer(m_stream, pipeWireBuffer);
}

uint64_t AbstractPipeWireStream::monotonicTimeNanoseconds() const
{
    timespec timestamp = {};
    if (clock_gettime(CLOCK_MONOTONIC, &timestamp) != 0) {
        return m_hasLastPresentationTime ? m_lastPresentationTime + 1 : 0;
    }
    return static_cast<uint64_t>(timestamp.tv_sec) * s_nanosecondsPerSecond
            + static_cast<uint64_t>(timestamp.tv_nsec);
}

uint64_t AbstractPipeWireStream::transactionPresentationTime(bool *valid) const
{
    *valid = false;
    const uint64_t maximumPts =
            static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
    if (!m_transaction
        || !m_transaction->timestampReceived
        || m_transaction->presentationNanoseconds >= s_nanosecondsPerSecond
        || m_transaction->presentationSeconds
                > (maximumPts - m_transaction->presentationNanoseconds)
                        / s_nanosecondsPerSecond) {
        return 0;
    }

    *valid = true;
    return m_transaction->presentationSeconds * s_nanosecondsPerSecond
            + m_transaction->presentationNanoseconds;
}

uint64_t AbstractPipeWireStream::normalizePresentationTime(uint64_t pts)
{
    const uint64_t maximumPts =
            static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
    pts = std::min(pts, maximumPts);
    if (m_hasLastPresentationTime && pts <= m_lastPresentationTime) {
        qCWarning(SCREENCAST) << "xdpw: non-monotonic presentation timestamp"
                              << pts << "after" << m_lastPresentationTime;
        pts = m_lastPresentationTime < maximumPts
                ? m_lastPresentationTime + 1
                : maximumPts;
    }

    m_lastPresentationTime = pts;
    m_hasLastPresentationTime = true;
    return pts;
}

bool AbstractPipeWireStream::isTerminalState() const
{
    return m_state == LifecycleState::Stopping
            || m_state == LifecycleState::Failed;
}

void AbstractPipeWireStream::failStream(const QString &error)
{
    if (m_failureEmitted || m_state == LifecycleState::Stopping) {
        return;
    }

    m_failureEmitted = true;
    m_state = LifecycleState::Failed;
    qCCritical(SCREENCAST) << "xdpw:" << error;
    Q_EMIT failed(error);
    emitClosed();
}

void AbstractPipeWireStream::emitClosed()
{
    if (m_closedEmitted
        || !m_readyEmitted
        || m_state == LifecycleState::Stopping) {
        return;
    }

    m_closedEmitted = true;
    Q_EMIT closed(m_nodeId);
}
