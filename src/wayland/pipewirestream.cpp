// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "pipewirestream.h"
#include "loggings.h"
#include "protocols/common.h"
#include "pipewirecore.h"
#include "pipewiretimer.h"
#include "pipewireutils.h"

#include <drm_fourcc.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <pipewire/pipewire.h>
#include <spa/buffer/meta.h>
#include <spa/utils/result.h>
#include <spa/param/props.h>
#include <spa/param/format-utils.h>
#include <spa/param/video/format-utils.h>
#include <spa/pod/dynamic.h>
#include <spa/param/video/format.h>
#include <spa/pod/builder.h>

#include <sys/mman.h>
#include <unistd.h>
#include <assert.h>
#include <libdrm/drm_fourcc.h>

#include <QGuiApplication>
#include <qpa/qplatformnativeinterface.h>
#include <QTimer>

#define XDP_SHOT_PROTO_VER 1
#define XDP_CAST_PROTO_VER 4

#define FPS_MEASURE_PERIOD_SEC 5.0

static void randname(char *buf) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    long r = ts.tv_nsec;
    for (int i = 0; i < 6; ++i) {
        assert(buf[i] == 'X');
        buf[i] = 'A'+(r&15)+(r&16)*2;
        r >>= 5;
    }
}

static int anonymous_shm_open(void) {
    char name[] = "/xdpw-shm-XXXXXX";
    int retries = 100;

    do {
        randname(name + strlen(name) - 6);

        --retries;
        // shm_open guarantees that O_CLOEXEC is set
        int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
        if (fd >= 0) {
            shm_unlink(name);
            return fd;
        }
    } while (retries > 0 && errno == EEXIST);

    return -1;
}

static struct spa_pod *fixate_format(struct spa_pod_builder *b, enum spa_video_format format,
                                     uint32_t width, uint32_t height, uint32_t framerate, uint64_t *modifier)
{
    struct spa_pod_frame f[1];

    enum spa_video_format format_without_alpha = PipeWireutils::pipewireFormatStripAlpha(format);

    spa_pod_builder_push_object(b, &f[0], SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat);
    spa_pod_builder_add(b, SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video), 0);
    spa_pod_builder_add(b, SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw), 0);
    /* format */
    if (modifier || format_without_alpha == SPA_VIDEO_FORMAT_UNKNOWN) {
        spa_pod_builder_add(b, SPA_FORMAT_VIDEO_format, SPA_POD_Id(format), 0);
    } else {
        spa_pod_builder_add(b, SPA_FORMAT_VIDEO_format,
                            SPA_POD_CHOICE_ENUM_Id(3, format, format, format_without_alpha), 0);
    }
    /* modifiers */
    if (modifier) {
        // implicit modifier
        spa_pod_builder_prop(b, SPA_FORMAT_VIDEO_modifier, SPA_POD_PROP_FLAG_MANDATORY);
        spa_pod_builder_long(b, *modifier);
    }
    struct spa_rectangle rect = SPA_RECTANGLE(width, height);
    spa_pod_builder_add(b, SPA_FORMAT_VIDEO_size,
                        SPA_POD_Rectangle(&rect), 0);

    struct spa_fraction fr0 = SPA_FRACTION(0, 1);
    spa_pod_builder_add(b, SPA_FORMAT_VIDEO_framerate,
                        SPA_POD_Fraction(&fr0), 0);

    struct spa_fraction fr1 = SPA_FRACTION(1, 1);
    struct spa_fraction fr_fps = SPA_FRACTION(framerate, 1);
    spa_pod_builder_add(b, SPA_FORMAT_VIDEO_maxFramerate,
                        SPA_POD_CHOICE_RANGE_Fraction(&fr_fps, &fr1, &fr_fps), 0);
    return static_cast<struct spa_pod *>(spa_pod_builder_pop(b, &f[0]));
}

static struct spa_pod *build_buffer(struct spa_pod_builder *b, uint32_t blocks, uint32_t size,
                                    uint32_t stride, uint32_t datatype) {
    assert(blocks > 0);
    assert(datatype > 0);
    struct spa_pod_frame f[1];

    spa_pod_builder_push_object(b, &f[0], SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers);
    spa_pod_builder_add(b, SPA_PARAM_BUFFERS_buffers,
                        SPA_POD_CHOICE_RANGE_Int(XDPW_PWR_BUFFERS, XDPW_PWR_BUFFERS_MIN, 32), 0);
    spa_pod_builder_add(b, SPA_PARAM_BUFFERS_blocks, SPA_POD_Int(blocks), 0);
    if (size > 0) {
        spa_pod_builder_add(b, SPA_PARAM_BUFFERS_size, SPA_POD_Int(size), 0);
    }
    if (stride > 0) {
        spa_pod_builder_add(b, SPA_PARAM_BUFFERS_stride, SPA_POD_Int(stride), 0);
    }
    spa_pod_builder_add(b, SPA_PARAM_BUFFERS_align, SPA_POD_Int(XDPW_PWR_ALIGN), 0);
    spa_pod_builder_add(b, SPA_PARAM_BUFFERS_dataType, SPA_POD_CHOICE_FLAGS_Int(datatype), 0);
    return static_cast<struct spa_pod *>(spa_pod_builder_pop(b, &f[0]));
}

static void handleStreamDestroy(void *data) {
    Q_UNUSED(data);
    qCDebug(SCREENCAST()) << "handleStreamDestroy";
}

static void handleStreamStateChanged(void *data,
                                     enum pw_stream_state old,
                                     enum pw_stream_state state,
                                     const char *error) {
    qCDebug(SCREENCAST, "pipewire: stream state changed to \"%s\"",
            pw_stream_state_as_string(state));
    PipeWireStream *cast = static_cast<PipeWireStream *>(data);
    cast->onStreamStateChanged(old, state, error);
}

static void handleStreamParamChanged(void *data, uint32_t id,
                                     const struct spa_pod *param) {
    qCDebug(SCREENCAST, "pipewire: stream parameters changed");
    PipeWireStream *cast = static_cast<PipeWireStream *>(data);
    cast->onStreamParamChanged(id, param);
}

static void handleStreamAddBuffer(void *data, struct pw_buffer *buffer) {
    qCDebug(SCREENCAST, "pipewire: add buffer event handle");
    PipeWireStream *cast = static_cast<PipeWireStream *>(data);
    cast->onStreamAddBuffer(buffer);
}

static void handleStreamRemoveBuffer(void *data, struct pw_buffer *buffer) {
    qCDebug(SCREENCAST, "pipewire: remove buffer event handle");
    PipeWireStream *cast = static_cast<PipeWireStream *>(data);
    cast->onStreamRemoveBuffer(buffer);
}

static void frameCaptureCallback(void *data) {
    PipeWireStream *cast = static_cast<PipeWireStream *>(data);
    cast->startframeCapture();
}

static void handleStreamOnProcess(void *data) {
    qCDebug(SCREENCAST, "pipewire: on process event handle");
    PipeWireStream *cast = static_cast<PipeWireStream *>(data);
    cast->onStreamProcess();
}

static const struct pw_stream_events pwr_stream_events = {
    .version = PW_VERSION_STREAM_EVENTS,
    .destroy = handleStreamDestroy,
    .state_changed = handleStreamStateChanged,
    .param_changed = handleStreamParamChanged,
    .add_buffer = handleStreamAddBuffer,
    .remove_buffer = handleStreamRemoveBuffer,
    .process = handleStreamOnProcess,
};

static struct spa_pod *build_format(struct spa_pod_builder *b, enum spa_video_format format,
                                    uint32_t width, uint32_t height, uint32_t framerate,
                                    uint64_t *modifiers, int modifier_count) {
    struct spa_pod_frame f[2];
    int i, c;

    enum spa_video_format format_without_alpha = PipeWireutils::pipewireFormatStripAlpha(format);

    spa_pod_builder_push_object(b, &f[0], SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat);
    spa_pod_builder_add(b, SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video), 0);
    spa_pod_builder_add(b, SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw), 0);
    if (modifier_count > 0 || format_without_alpha == SPA_VIDEO_FORMAT_UNKNOWN) {
        spa_pod_builder_add(b, SPA_FORMAT_VIDEO_format, SPA_POD_Id(format), 0);
    } else {
        spa_pod_builder_add(b, SPA_FORMAT_VIDEO_format,
                            SPA_POD_CHOICE_ENUM_Id(3, format, format, format_without_alpha), 0);
    }
    if (modifier_count > 0) {
        spa_pod_builder_prop(b, SPA_FORMAT_VIDEO_modifier, SPA_POD_PROP_FLAG_MANDATORY | SPA_POD_PROP_FLAG_DONT_FIXATE);
        spa_pod_builder_push_choice(b, &f[1], SPA_CHOICE_Enum, 0);
        for (i = 0, c = 0; i < modifier_count; i++) {
            spa_pod_builder_long(b, modifiers[i]);
            if (c++ == 0)
                spa_pod_builder_long(b, modifiers[i]);
        }
        spa_pod_builder_pop(b, &f[1]);
    }

    struct spa_rectangle rect = SPA_RECTANGLE(width, height);
    spa_pod_builder_add(b, SPA_FORMAT_VIDEO_size, SPA_POD_Rectangle(&rect), 0);

    struct spa_fraction zero = SPA_FRACTION(0, 1);
    spa_pod_builder_add(b, SPA_FORMAT_VIDEO_framerate, SPA_POD_Fraction(&zero), 0);

    struct spa_fraction fps = SPA_FRACTION(framerate, 1);
    struct spa_fraction one = SPA_FRACTION(1, 1);
    spa_pod_builder_add(b, SPA_FORMAT_VIDEO_maxFramerate,
                        SPA_POD_CHOICE_RANGE_Fraction(&fps, &one, &fps), 0);

    return static_cast<struct spa_pod *>(spa_pod_builder_pop(b, &f[0]));
}

PipeWireStream::PipeWireStream(QPointer<ScreenCastContext> context, QScreen *output, PortalCommon::CursorModes mode, QObject *parent)
    : QObject(parent)
    , m_context(context)
    , m_output(output)
    , m_sourceTYpe(PortalCommon::SourceType::Monitor)
    , m_mode(mode)
{
    m_framerate = output->refreshRate();
}

PipeWireStream::PipeWireStream(QPointer<ScreenCastContext> context, const QRect &region, PortalCommon::CursorModes mode, QObject *parent)
    : QObject(parent)
    , m_context(context)
    , m_sourceTYpe(PortalCommon::SourceType::Monitor)
    , m_mode(mode)
    , m_copyRegion(region)
{
}

PipeWireStream::~PipeWireStream()
{
    qCDebug(SCREENCAST) << "~PipeWireStream";
    destroyScreenCopyFrame();
    destroyStream();
}

void PipeWireStream::destroyPipeWireSourceBuffer(PipeWireSourceBuffer *buffer)
{
    wl_buffer_destroy(buffer->buffer);
    if (buffer->bufferType == PortalCommon::DMABUF) {
        gbm_bo_destroy(buffer->bo);
    }
    for (int plane = 0; plane < buffer->planeCount; plane++) {
        close(buffer->fd[plane]);
    }

    delete buffer;
}

void PipeWireStream::updateStreamParam()
{
    qCDebug(SCREENCAST, "pipewire: stream update parameters");
    uint8_t params_buffer[2][1024];
    struct spa_pod_dynamic_builder b[2];
    spa_pod_dynamic_builder_init(&b[0], params_buffer[0], sizeof(params_buffer[0]), 2048);
    spa_pod_dynamic_builder_init(&b[1], params_buffer[1], sizeof(params_buffer[1]), 2048);
    const struct spa_pod *params[2];

    struct spa_pod_builder *builder[2] = {&b[0].b, &b[1].b};
    uint32_t n_params = buildFormats(builder, params);

    pw_stream_update_params(m_stream, params, n_params);
    spa_pod_dynamic_builder_clean(&b[0]);
    spa_pod_dynamic_builder_clean(&b[1]);
}

void PipeWireStream::createStream()
{
    uint8_t buffer[2][1024];
    struct spa_pod_dynamic_builder b[2];
    spa_pod_dynamic_builder_init(&b[0], buffer[0], sizeof(buffer[0]), 2048);
    spa_pod_dynamic_builder_init(&b[1], buffer[1], sizeof(buffer[1]), 2048);
    const struct spa_pod *params[2];

    char name[] = "xdpw-stream-XXXXXX";
    randname(name + strlen(name) - 6);
    m_stream = pw_stream_new(m_context->m_pwCore->m_pwCore, name,
                           pw_properties_new(
                                   PW_KEY_MEDIA_CLASS, "Video/Source",
                                   nullptr));

    if (!m_stream) {
        qCFatal(SCREENCAST, "pipewire: failed to create stream");
    }
    m_isStreaming = false;

    struct spa_pod_builder *builder[2] = {&b[0].b, &b[1].b};
    uint32_t param_count = buildFormats(builder, params);
    spa_pod_dynamic_builder_clean(&b[0]);
    spa_pod_dynamic_builder_clean(&b[1]);

    pw_stream_add_listener(m_stream, &m_streamListener,
                           &pwr_stream_events, this);

    pw_stream_connect(m_stream,
                      PW_DIRECTION_OUTPUT,
                      PW_ID_ANY,
                      PW_STREAM_FLAG_ALLOC_BUFFERS,
                      params, param_count);
}

void PipeWireStream::destroyStream()
{
    if (!m_stream) {
        return;
    }

    qCDebug(SCREENCAST, "pipewire: destroying stream");
    pw_stream_flush(m_stream, false);
    pw_stream_disconnect(m_stream);
    pw_stream_destroy(m_stream);
    m_stream = nullptr;
}

void PipeWireStream::onStreamStateChanged(pw_stream_state old, pw_stream_state state, const char *error)
{
    switch (state) {
    case PW_STREAM_STATE_ERROR:
        qCCritical(SCREENCAST) << "stream error: " << error;
        break;
    case PW_STREAM_STATE_STREAMING:
        m_isStreaming = true;
        break;
    case PW_STREAM_STATE_PAUSED:
        if (m_nodeId == SPA_ID_INVALID && m_stream) {
            m_nodeId = pw_stream_get_node_id(m_stream);
            Q_EMIT ready(m_nodeId);
            qCDebug(SCREENCAST()) << "create node id" << nodeId();
        }
        if (old == PW_STREAM_STATE_STREAMING){
            enqueueBuffer();
        }

        m_isStreaming = false;
        break;
    case PW_STREAM_STATE_CONNECTING:
        break;
    case PW_STREAM_STATE_UNCONNECTED:
        Q_EMIT close(nodeId());
        break;
    }
}

void PipeWireStream::onStreamParamChanged(uint32_t id, const spa_pod *param)
{
    uint8_t params_buffer[3][1024];
    struct spa_pod_dynamic_builder b[3];
    const struct spa_pod *params[4];
    uint32_t blocks;
    uint32_t data_type;

    if (!param || id != SPA_PARAM_Format) {
        return;
    }

    spa_pod_dynamic_builder_init(&b[0], params_buffer[0], sizeof(params_buffer[0]), 2048);
    spa_pod_dynamic_builder_init(&b[1], params_buffer[1], sizeof(params_buffer[1]), 2048);
    spa_pod_dynamic_builder_init(&b[2], params_buffer[2], sizeof(params_buffer[2]), 2048);

    spa_format_video_raw_parse(param, &m_pipewireVideoInfo);
    m_framerate = (uint32_t)(m_pipewireVideoInfo.max_framerate.num / m_pipewireVideoInfo.max_framerate.denom);

    const struct spa_pod_prop *prop_modifier = spa_pod_find_prop(param, nullptr, SPA_FORMAT_VIDEO_modifier);
    if (prop_modifier) {
        m_bufferType = PortalCommon::DMABUF;
        data_type = 1<<SPA_DATA_DmaBuf;
        assert(m_pipewireVideoInfo.format == PipeWireutils::pipewireFormatFromDRMFormat(m_screencopyFrameInfo[PortalCommon::DMABUF].format));
        if ((prop_modifier->flags & SPA_POD_PROP_FLAG_DONT_FIXATE) > 0) {
            const struct spa_pod *pod_modifier = &prop_modifier->value;

            uint32_t n_modifiers = SPA_POD_CHOICE_N_VALUES(pod_modifier) - 1;
            uint64_t *modifiers = static_cast<uint64_t *>(SPA_POD_CHOICE_VALUES(pod_modifier));
            modifiers++;
            uint32_t flags = GBM_BO_USE_RENDERING;
            uint64_t modifier;
            uint32_t n_params;
            struct spa_pod_builder *builder[2] = {&b[0].b, &b[1].b};

            struct gbm_bo *bo = gbm_bo_create_with_modifiers2(m_context->m_gbmDevice,
                                                              m_screencopyFrameInfo[m_bufferType].width, m_screencopyFrameInfo[m_bufferType].height,
                                                              m_screencopyFrameInfo[m_bufferType].format, modifiers, n_modifiers, flags);
            if (bo) {
                modifier = gbm_bo_get_modifier(bo);
                gbm_bo_destroy(bo);
                goto fixate_format;
            }

            qCWarning(SCREENCAST, "pipewire: unable to allocate a dmabuf with modifiers. Falling back to the old api");
            for (uint32_t i = 0; i < n_modifiers; i++) {
                switch (modifiers[i]) {
                case DRM_FORMAT_MOD_INVALID:
                    flags = m_context->m_forceModLinear ?
                            GBM_BO_USE_RENDERING | GBM_BO_USE_LINEAR : GBM_BO_USE_RENDERING;
                    break;
                case DRM_FORMAT_MOD_LINEAR:
                    flags = GBM_BO_USE_RENDERING | GBM_BO_USE_LINEAR;
                    break;
                default:
                    continue;
                }
                bo = gbm_bo_create(m_context->m_gbmDevice,
                                   m_screencopyFrameInfo[m_bufferType].width, m_screencopyFrameInfo[m_bufferType].height,
                                   m_screencopyFrameInfo[m_bufferType].format, flags);
                if (bo) {
                    modifier = gbm_bo_get_modifier(bo);
                    gbm_bo_destroy(bo);
                    goto fixate_format;
                }
            }

            qCWarning(SCREENCAST, "pipewire: unable to allocate a dmabuf. Falling back to shm");
            m_avoidDMAbufs = true;

            n_params = buildFormats(builder, &params[0]);
            pw_stream_update_params(m_stream, params, n_params);
            spa_pod_dynamic_builder_clean(&b[0]);
            spa_pod_dynamic_builder_clean(&b[1]);
            spa_pod_dynamic_builder_clean(&b[2]);
            return;

        fixate_format:
            params[0] = fixate_format(&b[2].b, PipeWireutils::pipewireFormatFromDRMFormat(m_screencopyFrameInfo[m_bufferType].format),
                                      m_screencopyFrameInfo[m_bufferType].width, m_screencopyFrameInfo[m_bufferType].height, m_framerate, &modifier);

            n_params = buildFormats(builder, &params[1]);
            n_params++;

            pw_stream_update_params(m_stream, params, n_params);
            spa_pod_dynamic_builder_clean(&b[0]);
            spa_pod_dynamic_builder_clean(&b[1]);
            spa_pod_dynamic_builder_clean(&b[2]);
            return;
        }

        if (m_pipewireVideoInfo.modifier == DRM_FORMAT_MOD_INVALID) {
            blocks = 1;
        } else {
            blocks = gbm_device_get_format_modifier_plane_count(m_context->m_gbmDevice,
                                                                m_screencopyFrameInfo[PortalCommon::DMABUF].format, m_pipewireVideoInfo.modifier);
        }
    } else {
        m_bufferType = PortalCommon::SHM;
        blocks = 1;
        data_type = 1<<SPA_DATA_MemFd;
    }

    qCDebug(SCREENCAST, "pipewire: Format negotiated:");
    qCDebug(SCREENCAST, "pipewire: m_bufferType: %u (%u)", m_bufferType, data_type);
    qCDebug(SCREENCAST, "pipewire: format: %u", m_pipewireVideoInfo.format);
    qCDebug(SCREENCAST, "pipewire: modifier: %lu", m_pipewireVideoInfo.modifier);
    qCDebug(SCREENCAST, "pipewire: size: (%u, %u)", m_pipewireVideoInfo.size.width, m_pipewireVideoInfo.size.height);
    qCDebug(SCREENCAST, "pipewire: max_framerate: (%u / %u)", m_pipewireVideoInfo.max_framerate.num, m_pipewireVideoInfo.max_framerate.denom);

    params[0] = build_buffer(&b[0].b, blocks, m_screencopyFrameInfo[m_bufferType].size,
                             m_screencopyFrameInfo[m_bufferType].stride, data_type);

    params[1] = static_cast<struct spa_pod *>(spa_pod_builder_add_object(&b[1].b,
                                                                         SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta,
                                                                         SPA_PARAM_META_type, SPA_POD_Id(SPA_META_Header),
                                                                         SPA_PARAM_META_size, SPA_POD_Int(sizeof(struct spa_meta_header))));

    params[2] = static_cast<struct spa_pod *>(spa_pod_builder_add_object(&b[1].b,
                                                                         SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta,
                                                                         SPA_PARAM_META_type, SPA_POD_Id(SPA_META_VideoTransform),
                                                                         SPA_PARAM_META_size, SPA_POD_Int(sizeof(struct spa_meta_videotransform))));

    params[3] = static_cast<struct spa_pod *>(spa_pod_builder_add_object(&b[2].b,
                                                                         SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta,
                                                                         SPA_PARAM_META_type, SPA_POD_Id(SPA_META_VideoDamage),
                                                                         SPA_PARAM_META_size, SPA_POD_CHOICE_RANGE_Int(
                                                                                 sizeof(struct spa_meta_region) * 4,
                                                                                 sizeof(struct spa_meta_region) * 1,
                                                                                 sizeof(struct spa_meta_region) * 4)));

    pw_stream_update_params(m_stream, params, 4);
    spa_pod_dynamic_builder_clean(&b[0]);
    spa_pod_dynamic_builder_clean(&b[1]);
    spa_pod_dynamic_builder_clean(&b[2]);
}

void PipeWireStream::onStreamRemoveBuffer(pw_buffer *buffer)
{
    PipeWireStream::PipeWireSourceBuffer *PipeWireSourceBuffer =
            static_cast<PipeWireStream::PipeWireSourceBuffer *>(buffer->user_data);
    if (PipeWireSourceBuffer) {
        destroyPipeWireSourceBuffer(PipeWireSourceBuffer);
    }

    if (m_currentFrame.pw_buffer == buffer) {
        m_currentFrame.pw_buffer = nullptr;
    }

    for (uint32_t plane = 0; plane < buffer->buffer->n_datas; plane++) {
        buffer->buffer->datas[plane].fd = -1;
    }

    buffer->user_data = nullptr;
}

void PipeWireStream::onStreamAddBuffer(pw_buffer *buffer)
{
    struct spa_data *d;
    enum spa_data_type t;
    d = buffer->buffer->datas;

    if ((d[0].type & (1u << SPA_DATA_MemFd)) > 0) {
        assert(m_bufferType == PortalCommon::SHM);
        t = SPA_DATA_MemFd;
    } else if ((d[0].type & (1u << SPA_DATA_DmaBuf)) > 0) {
        assert(m_bufferType == PortalCommon::DMABUF);
        t = SPA_DATA_DmaBuf;
    } else {
        qCCritical(SCREENCAST) <<"unsupported buffer type" << d[0].type;
        m_err = 1;
        return;
    }

    qCDebug(SCREENCAST, "pipewire: selected buffertype %u", t);

    PipeWireStream::PipeWireSourceBuffer *PipeWireSourceBuffer = createPipeWireSourceBuffer(m_bufferType, &m_screencopyFrameInfo[m_bufferType]);
    if (!PipeWireSourceBuffer) {
        qCCritical(SCREENCAST, "pipewire: failed to create xdpw buffer");
        m_err = 1;
        return;
    }
    buffer->user_data = PipeWireSourceBuffer;

    assert(PipeWireSourceBuffer->planeCount >= 0 &&
           buffer->buffer->n_datas == (uint32_t)PipeWireSourceBuffer->planeCount);
    for (uint32_t plane = 0; plane < buffer->buffer->n_datas; plane++) {
        d[plane].type = t;
        d[plane].maxsize = PipeWireSourceBuffer->size[plane];
        d[plane].mapoffset = 0;
        d[plane].chunk->size = PipeWireSourceBuffer->size[plane];
        d[plane].chunk->stride = PipeWireSourceBuffer->stride[plane];
        d[plane].chunk->offset = PipeWireSourceBuffer->offset[plane];
        d[plane].flags = 0;
        d[plane].fd = PipeWireSourceBuffer->fd[plane];
        d[plane].data = nullptr;
        if (PipeWireSourceBuffer->bufferType == PortalCommon::DMABUF && d[plane].chunk->size == 0) {
            d[plane].chunk->size = 9;
        }
    }
}

void PipeWireStream::onStreamProcess()
{
    if (!m_isStreaming) {
        qCDebug(SCREENCAST, "pipewire: not streaming");
        return;
    }

    if (m_currentFrame.pw_buffer) {
        qCDebug(SCREENCAST, "pipewire: buffer already exported");
        return;
    }

    dequeueBuffer();
    if (!m_currentFrame.pw_buffer) {
        qCDebug(SCREENCAST, "pipewire: unable to export buffer");
        return;
    }

    if (m_seq > 0) {
        uint64_t delay_ns = fps_limit_measure_end(&fps_limit, m_framerate);
        if (delay_ns > 0) {
            qCDebug(SCREENCAST) << "seq:" << m_seq << "delay_ns" << delay_ns;
            if (delay_ns > 0) {
                xdpw_add_timer(&m_context->m_state, delay_ns,
                               (xdpw_event_loop_timer_func_t) frameCaptureCallback, this);
                return;
            }
        }
    }

    startframeCapture();
}

void PipeWireStream::enqueueBuffer()
{
    if (!m_currentFrame.pw_buffer) {
        qCWarning(SCREENCAST, "pipewire: no buffer to queue");
    } else {
        struct pw_buffer *pw_buf = m_currentFrame.pw_buffer;
        struct spa_buffer *spa_buf = pw_buf->buffer;
        struct spa_data *d = spa_buf->datas;

        bool buffer_corrupt = m_frameState != XDPW_FRAME_STATE_SUCCESS;

        if (m_currentFrame.y_invert) {
            //TODO: Flip buffer or set stride negative
            buffer_corrupt = true;
            m_err = 1;
        }

        struct spa_meta_header *h;
        if ((h = static_cast<struct spa_meta_header *>(spa_buffer_find_meta_data(spa_buf, SPA_META_Header, sizeof(*h))))) {
            h->pts = SPA_TIMESPEC_TO_NSEC(&m_currentFrame);
            h->flags = buffer_corrupt ? SPA_META_HEADER_FLAG_CORRUPTED : 0;
            h->seq = m_seq++;
            h->dts_offset = 0;
        }

        struct spa_meta_videotransform *vt;
        if ((vt = static_cast<struct spa_meta_videotransform *>(spa_buffer_find_meta_data(spa_buf, SPA_META_VideoTransform, sizeof(*vt))))) {
            vt->transform = m_currentFrame.transformation;
        }

        struct spa_meta *damage;
        if ((damage = spa_buffer_find_meta(spa_buf, SPA_META_VideoDamage))) {
            struct spa_region *d_region = static_cast<struct spa_region *>(spa_meta_first(damage));
            uint32_t damage_counter = 0;
            do {
                if (damage_counter >= m_currentFrame.damage_count) {
                    *d_region = SPA_REGION(0, 0, 0, 0);
                    qCDebug(SCREENCAST, "pipewire: end damage %u %u,%u (%ux%u)", damage_counter,
                            d_region->position.x, d_region->position.y, d_region->size.width, d_region->size.height);
                }
                QRect fdamage = m_currentFrame.damage[damage_counter];
                d_region->position.x = fdamage.x();
                d_region->position.y = fdamage.y();
                d_region->size.width = fdamage.width();
                d_region->size.height = fdamage.height();
                qCDebug(SCREENCAST, "pipewire: damage %u %u,%u (%ux%u)", damage_counter,
                        d_region->position.x, d_region->position.y, d_region->size.width, d_region->size.height);
            } while (spa_meta_check(d_region + 1, damage) && d_region++);

            if (damage_counter < m_currentFrame.damage_count) {
                QRect fdamage = QRect((uint32_t)d_region->position.x,
                                      (uint32_t)d_region->position.y,
                                      (uint32_t)d_region->size.width,
                                      (uint32_t)d_region->size.height);
                for (; damage_counter < m_currentFrame.damage_count; damage_counter++) {
                    fdamage |= m_currentFrame.damage[damage_counter];
                }
                d_region->position.x = fdamage.x();
                d_region->position.y = fdamage.y();
                d_region->size.width = fdamage.width();
                d_region->size.height = fdamage.height();
                qCDebug(SCREENCAST, "pipewire: collected damage %u %u,%u (%ux%u)", damage_counter,
                        d_region->position.x, d_region->position.y, d_region->size.width, d_region->size.height);
            }
        }

        if (buffer_corrupt) {
            for (uint32_t plane = 0; plane < spa_buf->n_datas; plane++) {
                d[plane].chunk->flags = SPA_CHUNK_FLAG_CORRUPTED;
            }
        } else {
            for (uint32_t plane = 0; plane < spa_buf->n_datas; plane++) {
                d[plane].chunk->flags = SPA_CHUNK_FLAG_NONE;
            }
        }

        for (uint32_t plane = 0; plane < spa_buf->n_datas; plane++) {
            qCDebug(SCREENCAST, "pipewire: plane %d", plane);
            qCDebug(SCREENCAST, "pipewire: fd %u", d[plane].fd);
            qCDebug(SCREENCAST, "pipewire: maxsize %d", d[plane].maxsize);
            qCDebug(SCREENCAST, "pipewire: size %d", d[plane].chunk->size);
            qCDebug(SCREENCAST, "pipewire: stride %d", d[plane].chunk->stride);
            qCDebug(SCREENCAST, "pipewire: offset %d", d[plane].chunk->offset);
            qCDebug(SCREENCAST, "pipewire: chunk flags %d", d[plane].chunk->flags);
        }
        qCDebug(SCREENCAST, "pipewire: width %d", m_currentFrame.PipeWireSourceBuffer->width);
        qCDebug(SCREENCAST, "pipewire: height %d", m_currentFrame.PipeWireSourceBuffer->height);
        qCDebug(SCREENCAST, "pipewire: y_invert %d", m_currentFrame.y_invert);
        qCDebug(SCREENCAST) << "pipewire: buffer type" << m_bufferType;
        int queueRet = pw_stream_queue_buffer(m_stream, pw_buf);
    }

    m_currentFrame.PipeWireSourceBuffer = nullptr;
    m_currentFrame.pw_buffer = nullptr;
}

void PipeWireStream::dequeueBuffer()
{
    assert(!m_currentFrame.pw_buffer);
    m_currentFrame.pw_buffer = pw_stream_dequeue_buffer(m_stream);
    if (!m_currentFrame.pw_buffer) {
        qCCritical(SCREENCAST, "pipewire: out of buffers");
        return;
    }

    m_currentFrame.PipeWireSourceBuffer = static_cast<PipeWireStream::PipeWireSourceBuffer *>(m_currentFrame.pw_buffer->user_data);
}

bool PipeWireStream::buildModifierlist(uint32_t drmFormat, uint64_t **modifiers, uint32_t *modifierCount)
{
    if (!m_context->queryDMABufModifiers(drmFormat,
                                         0,
                                         NULL,
                                         modifierCount)) {
        *modifiers = NULL;
        *modifierCount = 0;
        return false;
    }
    if (*modifierCount == 0) {
        qCWarning(SCREENCAST, "no modifiers available for format %u", drmFormat);
        *modifiers = NULL;
        return true;
    }
    *modifiers = static_cast<uint64_t*>(calloc(*modifierCount, sizeof(uint64_t)));
    bool ret = m_context->queryDMABufModifiers(drmFormat,
                                               *modifierCount,
                                               *modifiers,
                                               modifierCount);
    qCDebug(SCREENCAST, "num_modififiers %d", *modifierCount);
    return ret;
}

void PipeWireStream::handleFrameBufferDone()
{
    if (!m_initialized) {
        screenCopyFrameFinish();
        return;
    }

    if ((m_pipewireVideoInfo.format != PipeWireutils::pipewireFormatFromDRMFormat(m_screencopyFrameInfo[m_bufferType].format) &&
         m_pipewireVideoInfo.format != PipeWireutils::pipewireFormatStripAlpha(PipeWireutils::pipewireFormatFromDRMFormat(m_screencopyFrameInfo[m_bufferType].format))) ||
        m_pipewireVideoInfo.size.width != m_screencopyFrameInfo[m_bufferType].width ||
        m_pipewireVideoInfo.size.height != m_screencopyFrameInfo[m_bufferType].height) {
        qCWarning(SCREENCAST, "pipewire and screencopy metadata are incompatible. Renegotiate stream");
        m_frameState = XDPW_FRAME_STATE_RENEG;
        screenCopyFrameFinish();
        return;
    }

    if (!m_currentFrame.PipeWireSourceBuffer) {
        qCWarning(SCREENCAST, "no current buffer");
        screenCopyFrameFinish();
        return;
    }

    assert(m_currentFrame.PipeWireSourceBuffer);

    if ((m_bufferType == PortalCommon::SHM &&
         (m_currentFrame.PipeWireSourceBuffer->size[0] != m_screencopyFrameInfo[m_bufferType].size ||
          m_currentFrame.PipeWireSourceBuffer->stride[0] != m_screencopyFrameInfo[m_bufferType].stride)) ||
        m_currentFrame.PipeWireSourceBuffer->width != m_screencopyFrameInfo[m_bufferType].width ||
        m_currentFrame.PipeWireSourceBuffer->height != m_screencopyFrameInfo[m_bufferType].height) {
        qCWarning(SCREENCAST, "pipewire buffer has wrong dimensions");
        m_frameState = XDPW_FRAME_STATE_FAILED;
        screenCopyFrameFinish();
        return;
    }

    m_currentFrame.transformation = WL_OUTPUT_TRANSFORM_NORMAL;

    m_currentFrame.damage_count = 0;
    m_frame->copy_with_damage(m_currentFrame.PipeWireSourceBuffer->buffer);
    qCDebug(SCREENCAST, "ScreenCopyFrame buffer done, frame copied");

    fps_limit_measure_start(&fps_limit, m_framerate);
}

void PipeWireStream::screenCopyFrameFinish()
{
    destroyScreenCopyFrame();

    if (m_quit || m_err) {
        Q_EMIT failed("frame finish!");
        qCWarning(SCREENCAST, "finish screencopy failed");
        return;
    }

    if (!m_isStreaming) {
        m_frameState = XDPW_FRAME_STATE_NONE;
        qCDebug(SCREENCAST, "finish screencopy XDPW_FRAME_STATE_NONE");
        return;
    }

    if (m_frameState == XDPW_FRAME_STATE_RENEG) {
        qCDebug(SCREENCAST, "finish screencopy XDPW_FRAME_STATE_RENEG");
        updateStreamParam();
    }

    if (m_frameState == XDPW_FRAME_STATE_FAILED) {
        qCDebug(SCREENCAST, "finish screencopy XDPW_FRAME_STATE_FAILED");
        enqueueBuffer();
    }

    if (m_frameState == XDPW_FRAME_STATE_SUCCESS) {
        qCDebug(SCREENCAST, "finish screencopy XDPW_FRAME_STATE_SUCCESS");
        enqueueBuffer();
    }
}

void PipeWireStream::destroyScreenCopyFrame()
{
    if (!m_frame) {
        return;
    }

    QObject::disconnect(m_frame, nullptr, nullptr, nullptr);
    m_frame->destroy();
    delete m_frame;
    m_frame = nullptr;
}

void PipeWireStream::handleFrameBuffer(uint32_t format, uint32_t width, uint32_t height, uint32_t stride)
{
    m_screencopyFrameInfo[PortalCommon::SHM].width = width;
    m_screencopyFrameInfo[PortalCommon::SHM].height = height;
    m_screencopyFrameInfo[PortalCommon::SHM].stride = stride;
    m_screencopyFrameInfo[PortalCommon::SHM].size = stride * height;
    m_screencopyFrameInfo[PortalCommon::SHM].format = PipeWireutils::drmFormatfromWLShmFormat(static_cast<wl_shm_format>(format));

    if (m_context->m_screenCopyManager->version() < 3) {
        handleFrameBufferDone();
    }
}

void PipeWireStream::handleFrameFlags(uint32_t flags)
{
    m_currentFrame.y_invert = flags & ZWLR_SCREENCOPY_FRAME_V1_FLAGS_Y_INVERT;
}

void PipeWireStream::handleFrameReady(uint32_t tv_sec_hi,
                                      uint32_t tv_sec_lo,
                                      uint32_t tv_nsec)
{
    m_currentFrame.tv_sec = ((((uint64_t)tv_sec_hi) << 32) | tv_sec_lo);
    m_currentFrame.tv_nsec = tv_nsec;

    m_frameState = XDPW_FRAME_STATE_SUCCESS;

    screenCopyFrameFinish();
}

void PipeWireStream::handleFrameFailed()
{
    m_frameState = XDPW_FRAME_STATE_FAILED;
    screenCopyFrameFinish();
}

void PipeWireStream::handleFrameDamage(uint32_t x,
                                       uint32_t y,
                                       uint32_t width,
                                       uint32_t height)
{
    QRect damage = QRect(x, y, width, height);
    if (m_currentFrame.damage_count < 4) {
        m_currentFrame.damage[m_currentFrame.damage_count++] = damage;
    } else {
        m_currentFrame.damage[3] |= damage;
    }
}

void PipeWireStream::handleFrameLinuxDMABuf(uint32_t format,
                                            uint32_t width,
                                            uint32_t height)
{
    m_screencopyFrameInfo[PortalCommon::DMABUF].width = width;
    m_screencopyFrameInfo[PortalCommon::DMABUF].height = height;
    m_screencopyFrameInfo[PortalCommon::DMABUF].format = format;
}

uint32_t PipeWireStream::buildFormats(spa_pod_builder *b[2], const spa_pod *params[2])
{
    uint32_t param_count;
    uint32_t modifier_count;
    uint64_t *modifiers = NULL;

    if (!m_avoidDMAbufs &&
        buildModifierlist(m_screencopyFrameInfo[PortalCommon::DMABUF].format, &modifiers, &modifier_count) && modifier_count > 0) {
        param_count = 2;
        params[0] = build_format(b[0], PipeWireutils::pipewireFormatFromDRMFormat(m_screencopyFrameInfo[PortalCommon::DMABUF].format),
                                 m_screencopyFrameInfo[PortalCommon::DMABUF].width, m_screencopyFrameInfo[PortalCommon::DMABUF].height, m_framerate,
                                 modifiers, modifier_count);
        assert(params[0] != NULL);
        params[1] = build_format(b[1], PipeWireutils::pipewireFormatFromDRMFormat(m_screencopyFrameInfo[PortalCommon::SHM].format),
                                 m_screencopyFrameInfo[PortalCommon::SHM].width, m_screencopyFrameInfo[PortalCommon::SHM].height, m_framerate,
                                 NULL, 0);
        assert(params[1] != NULL);
    } else {
        param_count = 1;
        params[0] = build_format(b[0], PipeWireutils::pipewireFormatFromDRMFormat(m_screencopyFrameInfo[PortalCommon::SHM].format),
                                 m_screencopyFrameInfo[PortalCommon::SHM].width, m_screencopyFrameInfo[PortalCommon::SHM].height, m_framerate,
                                 NULL, 0);
        assert(params[0] != NULL);
    }
    free(modifiers);
    return param_count;
}

void PipeWireStream::startframeCapture()
{
    if (!m_context) {
        return;
    }

    if (!m_context->m_screenCopyManager->isActive()) {
        return;
    }

    frameCapture();
}

void PipeWireStream::frameCapture()
{
    if (m_quit || m_err) {
        // send failed, close stream
        Q_EMIT failed("frameCapture failed");
        return;
    }

    if (m_initialized && !m_isStreaming) {
        m_frameState = XDPW_FRAME_STATE_NONE;
        return;
    }
    m_frameState = XDPW_FRAME_STATE_STARTED;
    createScreencopyFrame();
}

void PipeWireStream::createScreencopyFrame()
{
    if (m_frame) {
        return;
    }

    if (m_sourceTYpe == PortalCommon::Monitor) {
        auto nativeInterface = qGuiApp->platformNativeInterface();
        auto *output = reinterpret_cast<wl_output *>(
                nativeInterface->nativeResourceForScreen(QByteArrayLiteral("output"), m_output));
        if (!output) {
            qCCritical(SCREENCAST) << "Could not find a matching Wayland output for screen";
            return;
        }
        m_frame = m_context->m_screenCopyManager->captureOutput(
                m_mode != PortalCommon::CursorModes::Hidden, output);
    }

    connect(m_frame, &ScreenCopyFrame::buffer, this,
            &PipeWireStream::handleFrameBuffer);
    connect(m_frame, &ScreenCopyFrame::bufferDone, this,
            &PipeWireStream::handleFrameBufferDone);
    connect(m_frame, &ScreenCopyFrame::linuxDmabuf, this,
            &PipeWireStream::handleFrameLinuxDMABuf);
    connect(m_frame, &ScreenCopyFrame::frameFlags, this,
            &PipeWireStream::handleFrameFlags);
    connect(m_frame, &ScreenCopyFrame::ready, this,
            &PipeWireStream::handleFrameReady);
    connect(m_frame, &ScreenCopyFrame::failed, this,
            &PipeWireStream::handleFrameFailed);
    connect(m_frame, &ScreenCopyFrame::damage, this,
            &PipeWireStream::handleFrameDamage);
}

int PipeWireStream::startScreencast()
{
    createScreencopyFrame();
    wl_display_dispatch(waylandDisplay()->wl_display());
    wl_display_roundtrip(waylandDisplay()->wl_display());

    if (m_screencopyFrameInfo[PortalCommon::SHM].format == DRM_FORMAT_INVALID
        || m_screencopyFrameInfo[PortalCommon::DMABUF].format == DRM_FORMAT_INVALID) {
        qCWarning(SCREENCAST) << "unable to reveive a valid format";
        return -1;
    }
    createStream();
    m_initialized = true;

    return 0;
}

PipeWireStream::PipeWireSourceBuffer *PipeWireStream::createPipeWireSourceBuffer(enum PortalCommon::BufferType bufferType, ScreenCopyFrameInfo *frameInfo)
{
    PipeWireSourceBuffer *buffer = new PipeWireSourceBuffer;
    buffer->width = frameInfo->width;
    buffer->height = frameInfo->height;
    buffer->format = frameInfo->format;
    buffer->bufferType = bufferType;

    switch (bufferType) {
    case PortalCommon::SHM:
        buffer->planeCount = 1;
        buffer->size[0] = frameInfo->size;
        buffer->stride[0] = frameInfo->stride;
        buffer->offset[0] = 0;
        buffer->fd[0] = anonymous_shm_open();
        if (buffer->fd[0] == -1) {
            qCCritical(SCREENCAST, "xdpw: unable to create anonymous filedescriptor");
            delete buffer;

            return nullptr;
        }

        if (ftruncate(buffer->fd[0], buffer->size[0]) < 0) {
            qCCritical(SCREENCAST, "unable to truncate filedescriptor");
            close(buffer->fd[0]);
            delete buffer;

            return nullptr;
        }

        buffer->buffer = m_context->createWLSHMBuffer(buffer->fd[0],
                                                      PipeWireutils::wlShmFormatFromDRMFormat(frameInfo->format),
                                                      frameInfo->width,
                                                      frameInfo->height,
                                                      frameInfo->stride);
        if (!buffer->buffer) {
            qCCritical(SCREENCAST, "unable to create wl_buffer");
            close(buffer->fd[0]);
            delete buffer;

            return nullptr;
        }
        break;
    case PortalCommon::DMABUF:;
        uint32_t flags = GBM_BO_USE_RENDERING;
        if (m_pipewireVideoInfo.modifier != DRM_FORMAT_MOD_INVALID) {
            uint64_t *modifiers = (uint64_t*)&m_pipewireVideoInfo.modifier;
            buffer->bo = gbm_bo_create_with_modifiers2(m_context->m_gbmDevice,
                                                       frameInfo->width,
                                                       frameInfo->height,
                                                       frameInfo->format,
                                                       modifiers,
                                                       1,
                                                       flags);
        } else {
            if (m_context->m_forceModLinear) {
                flags |= GBM_BO_USE_LINEAR;
            }
            buffer->bo = gbm_bo_create(m_context->m_gbmDevice,
                                       frameInfo->width,
                                       frameInfo->height,
                                       frameInfo->format,
                                       flags);
        }

        if (!buffer->bo&& m_pipewireVideoInfo.modifier == DRM_FORMAT_MOD_LINEAR) {
            buffer->bo = gbm_bo_create(m_context->m_gbmDevice,
                                       frameInfo->width,
                                       frameInfo->height,
                                       frameInfo->format,
                                       flags | GBM_BO_USE_LINEAR);
        }

        if (!buffer->bo) {
            qCCritical(SCREENCAST, "failed to create gbm_bo");
            delete buffer;

            return nullptr;
        }
        buffer->planeCount = gbm_bo_get_plane_count(buffer->bo);

        struct zwp_linux_buffer_params_v1 *params = m_context->m_linuxDmaBuf->create_params();
        if (!params) {
            qCCritical(SCREENCAST, "failed to create linux_buffer_params");
            gbm_bo_destroy(buffer->bo);
            delete buffer;

            return nullptr;
        }

        for (int plane = 0; plane < buffer->planeCount; plane++) {
            buffer->size[plane] = 0;
            buffer->stride[plane] = gbm_bo_get_stride_for_plane(buffer->bo, plane);
            buffer->offset[plane] = gbm_bo_get_offset(buffer->bo, plane);
            uint64_t mod = gbm_bo_get_modifier(buffer->bo);
            buffer->fd[plane] = gbm_bo_get_fd_for_plane(buffer->bo, plane);

            if (buffer->fd[plane] < 0) {
                qCCritical(SCREENCAST, "failed to get file descriptor");
                zwp_linux_buffer_params_v1_destroy(params);
                gbm_bo_destroy(buffer->bo);
                for (int plane_tmp = 0; plane_tmp < plane; plane_tmp++) {
                    close(buffer->fd[plane_tmp]);
                }

                delete buffer;

                return nullptr;
            }

            zwp_linux_buffer_params_v1_add(params,
                                           buffer->fd[plane],
                                           plane,
                                           buffer->offset[plane],
                                           buffer->stride[plane],
                                           mod >> 32, mod & 0xffffffff);
        }
        buffer->buffer = zwp_linux_buffer_params_v1_create_immed(params,
                                                                 buffer->width,
                                                                 buffer->height,
                                                                 buffer->format,
                                                                 0);
        zwp_linux_buffer_params_v1_destroy(params);

        if (!buffer->buffer) {
            qCCritical(SCREENCAST, "failed to create buffer");
            gbm_bo_destroy(buffer->bo);
            for (int plane = 0; plane < buffer->planeCount; plane++) {
                close(buffer->fd[plane]);
            }

            delete buffer;

            return nullptr;
        }
    }

    return buffer;
}
