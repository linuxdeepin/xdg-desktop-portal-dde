// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "abstractpipewirestream.h"
#include "loggings.h"
#include "protocols/common.h"
#include "pipewirecore.h"
#include "pipewireutils.h"

#include <pipewire/pipewire.h>
#include <pipewire/stream.h>
#include <spa/buffer/meta.h>
#include <spa/utils/result.h>
#include <spa/param/props.h>
#include <spa/param/format-utils.h>
#include <spa/param/video/format-utils.h>
#include <spa/pod/dynamic.h>
#include <spa/param/video/format.h>
#include <spa/pod/builder.h>

#include <unistd.h>
#include <assert.h>
#include <drm_fourcc.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <gbm.h>

#include <QScreen>
#include <QGuiApplication>

#define DAMAGE_REGION_COUNT 16

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
    AbstractPipeWireStream *stream = static_cast<AbstractPipeWireStream *>(data);
    stream->onStreamStateChanged(old, state, error);
}

static void handleStreamParamChanged(void *data, uint32_t id,
                                     const struct spa_pod *param) {
    qCDebug(SCREENCAST, "pipewire: stream parameters changed");
    AbstractPipeWireStream *stream = static_cast<AbstractPipeWireStream *>(data);
    stream->onStreamParamChanged(id, param);
}

static void handleStreamAddBuffer(void *data, struct pw_buffer *buffer) {
    qCDebug(SCREENCAST, "pipewire: add buffer event handle");
    AbstractPipeWireStream *stream = static_cast<AbstractPipeWireStream *>(data);
    stream->onStreamAddBuffer(buffer);
}

static void handleStreamRemoveBuffer(void *data, struct pw_buffer *buffer) {
    qCDebug(SCREENCAST, "pipewire: remove buffer event handle");
    AbstractPipeWireStream *stream = static_cast<AbstractPipeWireStream *>(data);
    stream->onStreamRemoveBuffer(buffer);
}

static void handleStreamOnProcess(void *data) {
    qCDebug(SCREENCAST, "pipewire: on process event handle");
    AbstractPipeWireStream *stream = static_cast<AbstractPipeWireStream *>(data);
    stream->onStreamProcess();
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

static struct spa_pod *buildFormat(struct spa_pod_builder *b, enum spa_video_format format,
                                    uint32_t width, uint32_t height, uint32_t framerate,
                                    uint64_t *modifiers, int modifier_count) {
    struct spa_pod_frame f[2];
    int i, c;

    enum spa_video_format format_without_alpha = PipeWireutils::pipewireFormatStripAlpha(format);

    spa_pod_builder_push_object(b, &f[0], SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat);
    spa_pod_builder_add(b, SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video), 0);
    spa_pod_builder_add(b, SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw), 0);
    /* format */
    if (modifier_count > 0 || format_without_alpha == SPA_VIDEO_FORMAT_UNKNOWN) {
        // modifiers are defined only in combinations with their format
        // we should not announce the format without alpha
        spa_pod_builder_add(b, SPA_FORMAT_VIDEO_format, SPA_POD_Id(format), 0);
    } else {
        spa_pod_builder_add(b, SPA_FORMAT_VIDEO_format,
                            SPA_POD_CHOICE_ENUM_Id(3, format, format, format_without_alpha), 0);
    }
    /* modifiers */
    if (modifier_count > 0) {
        // build an enumeration of modifiers
        spa_pod_builder_prop(b, SPA_FORMAT_VIDEO_modifier, SPA_POD_PROP_FLAG_MANDATORY | SPA_POD_PROP_FLAG_DONT_FIXATE);
        spa_pod_builder_push_choice(b, &f[1], SPA_CHOICE_Enum, 0);
        // modifiers from the array
        for (i = 0, c = 0; i < modifier_count; i++) {
            spa_pod_builder_long(b, modifiers[i]);
            if (c++ == 0)
                spa_pod_builder_long(b, modifiers[i]);
        }
        spa_pod_builder_pop(b, &f[1]);
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

static void addPod(struct wl_array *params, const struct spa_pod *pod) {
    if (pod) {
        const struct spa_pod **entry =
                static_cast<const struct spa_pod **>(wl_array_add(params, sizeof(**entry)));
        if (entry) {
            *entry = pod;
        }
    }
}

static struct spa_pod *fixateFormat(struct spa_pod_builder *b, enum spa_video_format format,
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

static struct spa_pod *buildBuffer(struct spa_pod_builder *b, uint32_t blocks, uint32_t size,
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

AbstractPipeWireStream::AbstractPipeWireStream(QPointer<ScreenCastContext> context,
                                               PortalCommon::CursorModes mode,
                                               QObject *parent)
    : QObject(parent)
    , m_context(context)
    , m_mode(mode)
    , m_timer(new QTimer(this))
{
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &AbstractPipeWireStream::handleTimeOut);
}

AbstractPipeWireStream::~AbstractPipeWireStream()
{
    qCDebug(SCREENCAST) << "~PipeWireStream";
    m_timer->stop();
    destroyImageCaptureFrame();
    if (m_session) {
        m_session->destroy();
        delete m_session;
    }

    if (m_source) {
        ext_image_capture_source_v1_destroy(m_source);
    }
    destroyStream();
    pipewireBufferConstraintsFinish(&m_currentConstraints);
    pipewireBufferConstraintsFinish(&m_pendingConstraints);
}

void AbstractPipeWireStream::onStreamStateChanged(pw_stream_state old, pw_stream_state state, const char *error)
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
        Q_EMIT closed(nodeId());
        break;
    }
}

void AbstractPipeWireStream::onStreamParamChanged(uint32_t id, const spa_pod *param)
{
    uint8_t params_buffer[3 * 1024];
    struct spa_pod_dynamic_builder builder;
    struct wl_array params;
    uint32_t blocks;
    uint32_t data_type;

    if (!param || id != SPA_PARAM_Format) {
        return;
    }
    wl_array_init(&params);
    spa_pod_dynamic_builder_init(&builder, params_buffer, sizeof(params_buffer), 2048);
    spa_format_video_raw_parse(param, &m_pipewireVideoInfo);

    m_framerate = (uint32_t)(m_pipewireVideoInfo.max_framerate.num / m_pipewireVideoInfo.max_framerate.denom);
    struct gbm_device *gbm = m_currentConstraints.gbm;
    const struct spa_pod_prop *prop_modifier = spa_pod_find_prop(param, nullptr, SPA_FORMAT_VIDEO_modifier);
    if (prop_modifier) {
        m_bufferType = PortalCommon::DMABUF;
        data_type = 1<<SPA_DATA_DmaBuf;
        uint32_t fourcc = PipeWireutils::drmFourccFromPipewireFormat(m_pipewireVideoInfo.format);
        assert(hasDrmFourcc(fourcc));
        if ((prop_modifier->flags & SPA_POD_PROP_FLAG_DONT_FIXATE) > 0) {
            const struct spa_pod *pod_modifier = &prop_modifier->value;

            uint32_t n_modifiers = SPA_POD_CHOICE_N_VALUES(pod_modifier) - 1;
            uint64_t *modifiers = static_cast<uint64_t *>(SPA_POD_CHOICE_VALUES(pod_modifier));
            modifiers++;
            uint32_t flags = GBM_BO_USE_RENDERING;
            uint64_t modifier;
            uint32_t n_params;

            struct gbm_bo *bo = gbm_bo_create_with_modifiers2(gbm,
                                                              m_currentConstraints.width,
                                                              m_currentConstraints.height,
                                                              fourcc, modifiers, n_modifiers, flags);
            if (bo) {
                modifier = gbm_bo_get_modifier(bo);
                gbm_bo_destroy(bo);
                goto fixate_format;
            }

            qCInfo(SCREENCAST, "pipewire: unable to allocate a dmabuf with modifiers. Falling back to the old api");
            for (uint32_t i = 0; i < n_modifiers; i++) {
                switch (modifiers[i]) {
                case DRM_FORMAT_MOD_INVALID:
                    flags = m_forceModLinear ?
                            GBM_BO_USE_RENDERING | GBM_BO_USE_LINEAR : GBM_BO_USE_RENDERING;
                    break;
                case DRM_FORMAT_MOD_LINEAR:
                    flags = GBM_BO_USE_RENDERING | GBM_BO_USE_LINEAR;
                    break;
                default:
                    continue;
                }
                bo = gbm_bo_create(gbm,
                                   m_currentConstraints.width,
                                   m_currentConstraints.height,
                                   fourcc, flags);
                if (bo) {
                    modifier = gbm_bo_get_modifier(bo);
                    gbm_bo_destroy(bo);
                    goto fixate_format;
                }
            }

            qCWarning(SCREENCAST, "pipewire: unable to allocate a dmabuf. Falling back to shm");
            m_avoidDMAbufs = true;

            buildFormats(&builder.b, &params);
            pw_stream_update_params(m_stream,
                                    static_cast<const struct spa_pod **>(params.data),
                                    params.size / sizeof(struct spa_pod *));
            spa_pod_dynamic_builder_clean(&builder);
            wl_array_release(&params);
            return;

        fixate_format:
            addPod(&params, fixateFormat(&builder.b,
                                           m_pipewireVideoInfo.format,
                                           m_currentConstraints.width,
                                           m_currentConstraints.height,
                                           m_framerate,
                                           &modifier));

            buildFormats(&builder.b, &params);

            pw_stream_update_params(m_stream,
                                    static_cast<const struct spa_pod **>(params.data),
                                    params.size / sizeof(struct spa_pod *));
            spa_pod_dynamic_builder_clean(&builder);
            wl_array_release(&params);
            return;
        }

        if (m_pipewireVideoInfo.modifier == DRM_FORMAT_MOD_INVALID) {
            blocks = 1;
        } else {
            blocks = gbm_device_get_format_modifier_plane_count(gbm,
                                                                fourcc,
                                                                m_pipewireVideoInfo.modifier);
        }
    } else {
        m_bufferType = PortalCommon::SHM;
        blocks = 1;
        data_type = 1<<SPA_DATA_MemFd;
    }

    qCDebug(SCREENCAST, "pipewire: Format negotiated:");
    qCDebug(SCREENCAST, "pipewire: bufferType: %u (%u)", m_bufferType, data_type);
    qCDebug(SCREENCAST, "pipewire: format: %u", m_pipewireVideoInfo.format);
    qCDebug(SCREENCAST, "pipewire: modifier: %lu", m_pipewireVideoInfo.modifier);
    qCDebug(SCREENCAST, "pipewire: size: (%u, %u)", m_pipewireVideoInfo.size.width, m_pipewireVideoInfo.size.height);
    qCDebug(SCREENCAST, "pipewire: max_framerate: (%u / %u)", m_pipewireVideoInfo.max_framerate.num, m_pipewireVideoInfo.max_framerate.denom);

    addPod(&params, buildBuffer(&builder.b, blocks, 0, 0, data_type));

    addPod(&params,  static_cast<struct spa_pod *>(spa_pod_builder_add_object(&builder.b,
                                                                              SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta,
                                                                              SPA_PARAM_META_type, SPA_POD_Id(SPA_META_Header),
                                                                              SPA_PARAM_META_size, SPA_POD_Int(sizeof(struct spa_meta_header)))));

    addPod(&params,  static_cast<struct spa_pod *>(spa_pod_builder_add_object(&builder.b,
                                                                              SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta,
                                                                              SPA_PARAM_META_type, SPA_POD_Id(SPA_META_VideoTransform),
                                                                              SPA_PARAM_META_size, SPA_POD_Int(sizeof(struct spa_meta_videotransform)))));

    addPod(&params,  static_cast<struct spa_pod *>(spa_pod_builder_add_object(&builder.b,
                                                                              SPA_TYPE_OBJECT_ParamMeta, SPA_PARAM_Meta,
                                                                              SPA_PARAM_META_type, SPA_POD_Id(SPA_META_VideoDamage),
                                                                              SPA_PARAM_META_size, SPA_POD_CHOICE_RANGE_Int(
                                                                                      sizeof(struct spa_meta_region) * DAMAGE_REGION_COUNT,
                                                                                      sizeof(struct spa_meta_region) * 1,
                                                                                      sizeof(struct spa_meta_region) * DAMAGE_REGION_COUNT))));

    pw_stream_update_params(m_stream,
                            static_cast<const struct spa_pod **>(params.data),
                            params.size / sizeof(struct spa_pod *));
    spa_pod_dynamic_builder_clean(&builder);
    wl_array_release(&params);
}

void AbstractPipeWireStream::onStreamRemoveBuffer(pw_buffer *buffer)
{
    PipeWireSourceBuffer *pipeWireSourceBuffer =
            static_cast<PipeWireSourceBuffer *>(buffer->user_data);

    if (pipeWireSourceBuffer) {
        m_buffers.removeOne(pipeWireSourceBuffer);
        destroyPipeWireSourceBuffer(pipeWireSourceBuffer);
    }

    if (m_currentFrame.pwBuffer == buffer) {
        m_currentFrame.pwBuffer = nullptr;
        m_currentFrame.pipeWireSourceBuffer = nullptr;
    }

    for (uint32_t plane = 0; plane < buffer->buffer->n_datas; plane++) {
        buffer->buffer->datas[plane].fd = -1;
    }

    buffer->user_data = nullptr;
}

void AbstractPipeWireStream::onStreamAddBuffer(pw_buffer *buffer)
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

    PipeWireSourceBuffer *pipeWireSourceBuffer = createPipeWireSourceBuffer(m_bufferType);
    if (!pipeWireSourceBuffer) {
        qCCritical(SCREENCAST, "pipewire: failed to create xdpw buffer");
        m_err = 1;
        return;
    }

    m_buffers.append(pipeWireSourceBuffer);
    buffer->user_data = pipeWireSourceBuffer;

    assert(pipeWireSourceBuffer->planeCount >= 0 &&
           buffer->buffer->n_datas == (uint32_t)pipeWireSourceBuffer->planeCount);
    for (uint32_t plane = 0; plane < buffer->buffer->n_datas; plane++) {
        d[plane].type = t;
        d[plane].maxsize = pipeWireSourceBuffer->size[plane];
        d[plane].mapoffset = 0;
        d[plane].chunk->size = pipeWireSourceBuffer->size[plane];
        d[plane].chunk->stride = pipeWireSourceBuffer->stride[plane];
        d[plane].chunk->offset = pipeWireSourceBuffer->offset[plane];
        d[plane].flags = 0;
        d[plane].fd = pipeWireSourceBuffer->fd[plane];
        d[plane].data = nullptr;
        if (pipeWireSourceBuffer->bufferType == PortalCommon::DMABUF && d[plane].chunk->size == 0) {
            d[plane].chunk->size = 9;
        }
    }
}

void AbstractPipeWireStream::onStreamProcess()
{
    if (!m_isStreaming) {
        qCDebug(SCREENCAST, "pipewire: not streaming");
        return;
    }

    if (m_currentFrame.pwBuffer) {
        qCDebug(SCREENCAST, "pipewire: buffer already exported");
        return;
    }

    dequeueBuffer();
    if (!m_currentFrame.pwBuffer) {
        qCDebug(SCREENCAST, "pipewire: unable to export buffer");
        return;
    }

    if (m_seq > 0) {
        uint64_t delay_ns = fps_limit_measure_end(&fps_limit, m_framerate);
        if (delay_ns > 0) {
            qCDebug(SCREENCAST) << "seq:" << m_seq << "delay_ns" << delay_ns << "framerate" << m_framerate;
            m_timer->start(delay_ns / 1000000);
            return;
        }
    }

    startframeCapture();
}

void AbstractPipeWireStream::enqueueBuffer()
{
    qCDebug(SCREENCAST, "pipewire: enqueue buffer");

    if (!m_currentFrame.pwBuffer) {
        qCWarning(SCREENCAST, "pipewire: no buffer to queue");
    } else {
        struct pw_buffer *pw_buf = m_currentFrame.pwBuffer;
        struct spa_buffer *spa_buf = pw_buf->buffer;
        struct spa_data *d = spa_buf->datas;

        bool buffer_corrupt = m_frameState != XDPW_FRAME_STATE_SUCCESS;

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
            bool stopped_for_spa = false;
            for(QRegion::const_iterator it = m_currentFrame.pipeWireSourceBuffer->damage.begin(); it != m_currentFrame.pipeWireSourceBuffer->damage.end(); ++it) {
                const QRect &rect = *it;
                if (!spa_meta_check(d_region + 1, damage)) {
                    stopped_for_spa = true;
                    break;
                }
                d_region++;

                *d_region = SPA_REGION(rect.x(),
                                       rect.y(),
                                       rect.width(),
                                       rect.height());
                qCDebug(SCREENCAST, "pipewire: damage %u %u,%u (%ux%u)", damage_counter,
                         d_region->position.x, d_region->position.y, d_region->size.width, d_region->size.height);
                damage_counter++;
            }
            if (stopped_for_spa) {
                QRegion new_fdamage = QRegion(d_region->position.x, d_region->position.y, d_region->size.width, d_region->size.height);

                for(QRegion::const_iterator it = m_currentFrame.pipeWireSourceBuffer->damage.begin(); it != m_currentFrame.pipeWireSourceBuffer->damage.end(); ++it) {
                    const QRect &rect = *it;
                    if (damage_counter-- > 0) {
                        continue;
                    }
                    new_fdamage += rect;
                }
                *d_region = SPA_REGION(new_fdamage.boundingRect().x(),
                                       new_fdamage.boundingRect().y(),
                                       new_fdamage.boundingRect().width(),
                                       new_fdamage.boundingRect().height());
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
        qCDebug(SCREENCAST, "pipewire: width %d", m_currentFrame.pipeWireSourceBuffer->width);
        qCDebug(SCREENCAST, "pipewire: height %d", m_currentFrame.pipeWireSourceBuffer->height);
        qCDebug(SCREENCAST, "pipewire: format %d", m_pipewireVideoInfo.format);
        qCDebug(SCREENCAST) << "pipewire: buffer type" << m_bufferType;
        pw_stream_queue_buffer(m_stream, pw_buf);
    }

    m_currentFrame.pipeWireSourceBuffer = nullptr;
    m_currentFrame.pwBuffer = nullptr;
}

void AbstractPipeWireStream::dequeueBuffer()
{
    qCDebug(SCREENCAST, "pipewire: dequeueing buffer");

    assert(!m_currentFrame.pwBuffer);
    if (!(m_currentFrame.pwBuffer = pw_stream_dequeue_buffer(m_stream))) {
        qCWarning(SCREENCAST, "pipewire: out of buffers");
        return;
    }

    m_currentFrame.pipeWireSourceBuffer = static_cast<PipeWireSourceBuffer *>(m_currentFrame.pwBuffer->user_data);
}

void AbstractPipeWireStream::buildFormats(spa_pod_builder *builder, wl_array *params)
{
    if (!m_avoidDMAbufs) {
        uint32_t last_format = DRM_FORMAT_INVALID;
        foreach (struct DRMFormatModifierPair *formatPairvar, std::as_const(m_currentConstraints.dmabuf_format_modifier_pairs)) {
            if (last_format == formatPairvar->fourcc) {
                continue;
            }
            enum spa_video_format pw_format = PipeWireutils::pipewireFormatFromDRMFormat(formatPairvar->fourcc);
            if (pw_format == SPA_VIDEO_FORMAT_UNKNOWN) {
                continue;
            }
            last_format = formatPairvar->fourcc;

            uint32_t modifier_count;
            uint64_t *modifiers = nullptr;
            buildModifierList(formatPairvar->fourcc, &modifiers, &modifier_count);
            if (modifier_count > 0) {
                addPod(params, buildFormat(builder, pw_format,
                                             m_currentConstraints.width, m_currentConstraints.height,
                                             m_framerate, modifiers, modifier_count));
            }
            free(modifiers);
        }
    }

    foreach (struct xdpw_shm_format *format, std::as_const(m_currentConstraints.shm_formats)) {
        enum spa_video_format pw_format = PipeWireutils::pipewireFormatFromDRMFormat(format->fourcc);
        if (pw_format != SPA_VIDEO_FORMAT_UNKNOWN) {
            addPod(params, buildFormat(builder, pw_format,
                                         m_currentConstraints.width, m_currentConstraints.height,
                                         m_framerate, nullptr, 0));
        }
    }
}

void AbstractPipeWireStream::pipewireBufferConstraintsInit(struct PipewireBufferConstraints *constraints)
{
    constraints->dirty = false;
    constraints->width = 0;
    constraints->height = 0;
    constraints->dmabuf_format_modifier_pairs.clear();
    constraints->shm_formats.clear();
    constraints->gbm = nullptr;
}

bool AbstractPipeWireStream::pipewireBufferConstraintsMove(PipewireBufferConstraints *dst, PipewireBufferConstraints *src)
{
    if (!src->dirty) {
        return false;
    }
    int dirty = src->dirty;

    pipewireBufferConstraintsFinish(dst);
    dst->dirty = src->dirty;
    dst->width = src->width;
    dst->height = src->height;
    dst->dmabuf_format_modifier_pairs = src->dmabuf_format_modifier_pairs;
    dst->shm_formats = src->shm_formats;
    dst->gbm = src->gbm;

    pipewireBufferConstraintsInit(src);
    return dirty;
}

void AbstractPipeWireStream::pipewireBufferConstraintsFinish(PipewireBufferConstraints *constraints)
{
    foreach (struct xdpw_shm_format *fmt, constraints->shm_formats) {
        delete fmt;
    }
    foreach (struct DRMFormatModifierPair *formatPair, m_currentConstraints.dmabuf_format_modifier_pairs) {
        delete formatPair;
    }

    constraints->dmabuf_format_modifier_pairs.clear();
    constraints->shm_formats.clear();
    if (constraints->gbm) {
        int fd = gbm_device_get_fd(constraints->gbm);
        gbm_device_destroy(constraints->gbm);
        close(fd);
    }
    constraints->gbm = nullptr;
    constraints->dirty = false;
    constraints->height = 0;
    constraints->width = 0;
}

void AbstractPipeWireStream::frameCapture()
{
    qCDebug(SCREENCAST) << "pipewire: frameCapture";
    if (!m_currentFrame.pipeWireSourceBuffer) {
        qCDebug(SCREENCAST, "ext: started frame without buffer");
        return;
    }

    if (m_quit || m_err) {
        qCDebug(SCREENCAST) << "frameCapture failed" << "m_quit:" << m_quit << "m_err:" << m_err;
        Q_EMIT failed("frameCapture failed");
        return;
    }

    if (m_initialized && !m_isStreaming) {
        qCDebug(SCREENCAST) << "state error," << "m_initialized:"  << m_initialized << "m_isStreaming:" << m_isStreaming;
        m_frameState = XDPW_FRAME_STATE_NONE;
        return;
    }

    m_frameState = XDPW_FRAME_STATE_STARTED;
    createImageCaptureFrame();
}

void AbstractPipeWireStream::createImageCaptureFrame()
{
    qCDebug(SCREENCAST) << "pipewire: createImageCaptureFrame" << m_frame;
    if (m_frame)
        return;

    m_frame = new ImageCopyCaptureFrame(m_session->create_frame());
    connect(m_frame, &ImageCopyCaptureFrame::transformChanged, this,
            &AbstractPipeWireStream::handleFrameTransform);
    connect(m_frame, &ImageCopyCaptureFrame::damaged, this,
            &AbstractPipeWireStream::handleFrameDamage);
    connect(m_frame, &ImageCopyCaptureFrame::presentationTimeChanged, this,
            &AbstractPipeWireStream::handleFramePresentationTime);
    connect(m_frame, &ImageCopyCaptureFrame::ready, this,
            &AbstractPipeWireStream::handleFrameReady);
    connect(m_frame, &ImageCopyCaptureFrame::failed, this,
            &AbstractPipeWireStream::handleFrameFailed);

    if (m_currentFrame.pipeWireSourceBuffer) {
        m_frame->attach_buffer(m_currentFrame.pipeWireSourceBuffer->buffer);
        foreach (PipeWireSourceBuffer *buffer, std::as_const(m_buffers)) {
            for(QRegion::const_iterator it = buffer->damage.begin(); it != buffer->damage.end(); ++it) {
                const QRect &rect = *it;
                m_frame->damaged(rect.x(), rect.y(), rect.width(), rect.height());
            }
        }

        m_frame->capture();
    }
}

void AbstractPipeWireStream::destroyImageCaptureFrame()
{
    if (!m_frame) {
        return;
    }

    QObject::disconnect(m_frame, nullptr, nullptr, nullptr);
    m_frame->destroy();
    delete m_frame;
    m_frame = nullptr;
}

void AbstractPipeWireStream::finishImageCaptureFrame()
{
    destroyImageCaptureFrame();

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

void AbstractPipeWireStream::handleCaptureSessionBufferSizeChanged(uint32_t width, uint32_t height)
{
    m_pendingConstraints.width = width;
    m_pendingConstraints.height = height;
    m_pendingConstraints.dirty = true;
}

void AbstractPipeWireStream::handleCaptureSessionShmFormatChanged(uint32_t format)
{
    uint32_t fourcc = PipeWireutils::drmFormatfromWLShmFormat(static_cast<wl_shm_format>(format));

    char *fmt_name = drmGetFormatName(fourcc);
    struct xdpw_shm_format *fmt;
    foreach (struct xdpw_shm_format *fmt, std::as_const(m_pendingConstraints.shm_formats)) {
        if (fmt->fourcc == fourcc) {
            qCDebug(SCREENCAST, "ext: skipping duplicated format: %s (%X)", fmt_name, fourcc);
            free(fmt_name);
            return;
        }
    }

    if (PipeWireutils::pipewireBPPFromDrmFourcc(fourcc) <= 0) {
        qCDebug(SCREENCAST, "ext: unsupported shm format: %s (%X)", fmt_name, fourcc);
        return;
    }

    struct xdpw_shm_format *newFmt = new xdpw_shm_format;
    newFmt->fourcc = fourcc;
    newFmt->stride = 0;
    m_pendingConstraints.dirty = true;
    m_pendingConstraints.shm_formats.append(newFmt);
    qCDebug(SCREENCAST, "ext: shm_format: %s (%X)", fmt_name, fourcc);
    free(fmt_name);
}

void AbstractPipeWireStream::handleCaptureSessionDmabufDeviceChanged(wl_array *device_arr)
{
    dev_t device;
    assert(device_arr->size == sizeof(device));
    memcpy(&device, device_arr->data, sizeof(device));

    drmDevice *drmDev;
    if (drmGetDeviceFromDevId(device, /* flags */ 0, &drmDev) != 0) {
        m_forceModLinear = true;
        return;
    }

    m_pendingConstraints.gbm = ScreenCastContext::createGBMDeviceFromDRMDevice(drmDev);
    m_pendingConstraints.dirty = true;
    qCDebug(SCREENCAST, "ext: dmabuf_device handler");
}

void AbstractPipeWireStream::handleCaptureSessionDmabufFormatChanged(uint32_t format, wl_array *modifiers)
{
    char *fmt_name = drmGetFormatName(format);
    void *p;
    wl_array_for_each(p, modifiers) {
        uint64_t *modifier = (uint64_t *)(p);
        bool newPair = true;
        foreach (struct DRMFormatModifierPair *tmp, std::as_const(m_pendingConstraints.dmabuf_format_modifier_pairs)) {
            if (tmp->fourcc == format && tmp->modifier == *modifier) {
                newPair = false;
                break;
            }
        }

        if (!newPair) {
            qCDebug(SCREENCAST, "ext: skipping duplicated format %s (%X, %lu)", fmt_name, format, *modifier);
            continue;
        }

        struct DRMFormatModifierPair *fm_pair = new DRMFormatModifierPair;
        fm_pair->fourcc = format;
        fm_pair->modifier = *modifier;
        m_pendingConstraints.dmabuf_format_modifier_pairs.append(fm_pair);

        char *modifier_name = drmGetFormatModifierName(*modifier);
        qCDebug(SCREENCAST, "ext: dmabuf_format handler: %s (%X), modifier: %s (%X)",
                fmt_name,
                format,
                modifier_name,
                *modifier);
        free(modifier_name);
    }

    m_pendingConstraints.dirty = true;
    free(fmt_name);
}

void AbstractPipeWireStream::handleCaptureSessionDone()
{
    qCDebug(SCREENCAST, "ext: done handler");

    foreach (struct xdpw_shm_format *fmt, m_pendingConstraints.shm_formats) {
        int bpp = PipeWireutils::pipewireBPPFromDrmFourcc(fmt->fourcc);
        assert(bpp > 0);
        fmt->stride = bpp * m_pendingConstraints.width;
    }

    if (pipewireBufferConstraintsMove(&m_currentConstraints, &m_pendingConstraints)) {
        qCDebug(SCREENCAST, "ext: buffer constraints changed");
        updateStreamParam();
        return;
    }
}

void AbstractPipeWireStream::handleCaptureSessionStopped()
{
    qCCritical(SCREENCAST, "ext: session_stopped handler");
    Q_EMIT failed("capture session stopped");
}

void AbstractPipeWireStream::handleFrameTransform(uint32_t transform)
{
    qCDebug(SCREENCAST, "ext: transform handler %u", transform);
    m_currentFrame.transformation = transform;
}

void AbstractPipeWireStream::handleFrameDamage(int32_t x, int32_t y, int32_t width, int32_t height)
{
    qCDebug(SCREENCAST) << "ext: damage:" << x << y << width << height;

    foreach(PipeWireSourceBuffer *buffer, m_buffers) {
        buffer->damage += QRect(x, y, width, height);
    }
}

void AbstractPipeWireStream::handleFramePresentationTime(uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec)
{
    m_currentFrame.tv_sec = ((((uint64_t)tv_sec_hi) << 32) | tv_sec_lo);
    m_currentFrame.tv_nsec = tv_nsec;

    qCDebug(SCREENCAST) << "ext: timestamp" << m_currentFrame.tv_sec << ":" << m_currentFrame.tv_nsec ;
}

void AbstractPipeWireStream::handleFrameReady()
{
    fps_limit_measure_start(&fps_limit, m_framerate);
    m_frameState = XDPW_FRAME_STATE_SUCCESS;
    destroyImageCaptureFrame();
    PipeWireSourceBuffer *buffer = m_currentFrame.pipeWireSourceBuffer;
    enqueueBuffer();
    if (buffer) {
        buffer->damage = QRegion();
    }
}

void AbstractPipeWireStream::handleFrameFailed(uint32_t reason)
{
    m_frameState = XDPW_FRAME_STATE_FAILED;
    destroyImageCaptureFrame();

    switch (reason) {
    case EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_UNKNOWN:
        qCCritical(SCREENCAST, "ext: frame capture failed: unknown reason");
        return;
    case EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_BUFFER_CONSTRAINTS:
        qCCritical(SCREENCAST, "ext: frame capture failed: buffer constraint mismatch");
        enqueueBuffer();
        return;
    case EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_STOPPED:
        qCCritical(SCREENCAST, "ext: frame capture failed: capture session stopped");
        return;
    default:
        qCCritical(SCREENCAST, "ext: frame capture failed: undefined failed reason");
    }
}

void AbstractPipeWireStream::handleTimeOut()
{
    startframeCapture();
}

void AbstractPipeWireStream::updateStreamParam()
{
    qCDebug(SCREENCAST, "pipewire: stream update parameters");
    if (!m_stream) {
        return;
    }
    uint8_t params_buffer[2048];
    struct spa_pod_dynamic_builder builder;
    spa_pod_dynamic_builder_init(&builder, params_buffer, sizeof(params_buffer[0]), 2048);

    struct wl_array params;
    wl_array_init(&params);
    buildFormats(&builder.b, &params);

    pw_stream_update_params(m_stream, static_cast<const struct spa_pod **>(params.data), params.size / sizeof(struct spa_pod *));
    spa_pod_dynamic_builder_clean(&builder);
    wl_array_release(&params);
}

void AbstractPipeWireStream::createStream()
{
    uint8_t buffer[2 * 1024];
    struct spa_pod_dynamic_builder builder;
    spa_pod_dynamic_builder_init(&builder, buffer, sizeof(buffer), 2048);

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

    struct wl_array params;
    wl_array_init(&params);
    buildFormats(&builder.b, &params);

    fps_limit_measure_start(&fps_limit, m_framerate);
    pw_stream_add_listener(m_stream, &m_streamListener,
                           &pwr_stream_events, this);

    pw_stream_connect(m_stream,
                      PW_DIRECTION_OUTPUT,
                      PW_ID_ANY,
                      PW_STREAM_FLAG_ALLOC_BUFFERS,
                      static_cast<const struct spa_pod **>(params.data),
                      params.size / sizeof(struct spa_pod *));

    spa_pod_dynamic_builder_clean(&builder);
    wl_array_release(&params);
}

void AbstractPipeWireStream::destroyStream()
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

void AbstractPipeWireStream::buildModifierList(uint32_t drm_format, uint64_t **modifiers, uint32_t *modifier_count)
{
    *modifier_count = countDmabufModifiers(drm_format);
    if (*modifier_count == 0) {
        qCWarning(SCREENCAST, "wlroots: no modifiers available for format %u", drm_format);
        *modifiers = nullptr;
        return;
    }

    *modifiers = static_cast<uint64_t*>(calloc(*modifier_count, sizeof(uint64_t)));
    queryDmabufModifiers(drm_format, *modifiers, *modifier_count);
    qCDebug(SCREENCAST) << "wlroots: num_modifiers" << modifier_count;
}

void AbstractPipeWireStream::queryDmabufModifiers(uint32_t drm_format, uint64_t *modifiers, uint32_t num_modifiers)
{
    uint32_t idx = 0;
    struct gbm_device *gbm = m_currentConstraints.gbm;
    foreach (struct DRMFormatModifierPair *fm_pair, std::as_const(m_currentConstraints.dmabuf_format_modifier_pairs)) {
        if (fm_pair->fourcc == drm_format &&
            (fm_pair->modifier == DRM_FORMAT_MOD_INVALID ||
             gbm_device_get_format_modifier_plane_count(gbm, fm_pair->fourcc, fm_pair->modifier) > 0)) {
            assert(idx < num_modifiers);
            modifiers[idx] = fm_pair->modifier;
            idx++;
        }
    }
}

uint32_t AbstractPipeWireStream::countDmabufModifiers(uint32_t drm_format)
{
    struct PipewireBufferConstraints *constraints = &m_currentConstraints;

    uint32_t modifiers = 0;
    struct gbm_device *gbm = m_currentConstraints.gbm;
    foreach (struct DRMFormatModifierPair *fm_pair, std::as_const(constraints->dmabuf_format_modifier_pairs)) {
        if (fm_pair->fourcc == drm_format &&
            (fm_pair->modifier == DRM_FORMAT_MOD_INVALID ||
             gbm_device_get_format_modifier_plane_count(gbm, fm_pair->fourcc, fm_pair->modifier) > 0))
            modifiers += 1;
    }

    return modifiers;
}

PipeWireSourceBuffer *AbstractPipeWireStream::createPipeWireSourceBuffer(PortalCommon::BufferType bufferType)
{
    PipeWireSourceBuffer *buffer = new PipeWireSourceBuffer;

    uint32_t format = PipeWireutils::drmFourccFromPipewireFormat(m_pipewireVideoInfo.format);
    assert(format != DRM_FORMAT_INVALID);

    buffer->width = m_currentConstraints.width;
    buffer->height = m_currentConstraints.height;
    buffer->bufferType = bufferType;
    buffer->format = format;

    struct xdpw_shm_format *fmt = nullptr;
    bool found = false;
    struct gbm_device *gbm = m_currentConstraints.gbm;

    switch (bufferType) {
    case PortalCommon::SHM:
        foreach (struct xdpw_shm_format *tmp, m_currentConstraints.shm_formats) {
            if (tmp->fourcc == format) {
                found = true;
                fmt = tmp;
                break;
            }
        }
        if (!found) {
            qCCritical(PIPEWIRE, "xdpw: unable to find format: %d", format);
            destroyPipeWireSourceBuffer(buffer);
            return nullptr;
        }

        buffer->planeCount = 1;
        buffer->size[0] = fmt->stride * buffer->height;
        buffer->stride[0] = fmt->stride;
        buffer->offset[0] = 0;
        buffer->fd[0] = anonymous_shm_open();
        if (buffer->fd[0] == -1) {
            qCCritical(PIPEWIRE, "xdpw: unable to create anonymous filedescriptor");
            destroyPipeWireSourceBuffer(buffer);
            return nullptr;
        }

        if (ftruncate(buffer->fd[0], buffer->size[0]) < 0) {
            qCCritical(PIPEWIRE, "xdpw: unable to truncate filedescriptor");
            destroyPipeWireSourceBuffer(buffer);
            return nullptr;
        }

        buffer->buffer =  m_context->createWLSHMBuffer(buffer->fd[0],
                                                      PipeWireutils::wlShmFormatFromDRMFormat(format),
                                                      buffer->width,
                                                      buffer->height,
                                                      fmt->stride);

        if (!buffer->buffer) {
            qCCritical(PIPEWIRE, "xdpw: unable to create wl_buffer");
            close(buffer->fd[0]);
            delete buffer;
            return nullptr;
        }
        break;
    case PortalCommon::DMABUF:;
        struct gbm_bo *bo;
        uint32_t flags = GBM_BO_USE_RENDERING;
        if (m_pipewireVideoInfo.modifier != DRM_FORMAT_MOD_INVALID) {
            uint64_t *modifiers = (uint64_t*)&m_pipewireVideoInfo.modifier;
            bo = gbm_bo_create_with_modifiers2(gbm, buffer->width, buffer->height,
                                               format, modifiers, 1, flags);
        } else {
            if (m_forceModLinear) {
                flags |= GBM_BO_USE_LINEAR;
            }
            bo = gbm_bo_create(gbm, buffer->width, buffer->height, format, flags);
        }

        if (!bo && m_pipewireVideoInfo.modifier == DRM_FORMAT_MOD_LINEAR) {
            bo = gbm_bo_create(gbm, buffer->width, buffer->height,
                               format, flags | GBM_BO_USE_LINEAR);
        }

        if (!bo) {
            qCCritical(PIPEWIRE, "xdpw: failed to create gbm_bo");
            destroyPipeWireSourceBuffer(buffer);
            return nullptr;
        }
        buffer->planeCount = gbm_bo_get_plane_count(bo);

        struct zwp_linux_buffer_params_v1 *params = m_context->m_linuxDmaBuf->create_params();
        if (!params) {
            qCCritical(SCREENCAST, "failed to create linux_buffer_params");
            gbm_bo_destroy(bo);
            destroyPipeWireSourceBuffer(buffer);

            return nullptr;
        }

        for (int plane = 0; plane < buffer->planeCount; plane++) {
            buffer->size[plane] = 0;
            buffer->stride[plane] = gbm_bo_get_stride_for_plane(bo, plane);
            buffer->offset[plane] = gbm_bo_get_offset(bo, plane);
            uint64_t mod = gbm_bo_get_modifier(bo);
            buffer->fd[plane] = gbm_bo_get_fd_for_plane(bo, plane);

            if (buffer->fd[plane] < 0) {
                qCCritical(SCREENCAST, "failed to get file descriptor");
                zwp_linux_buffer_params_v1_destroy(params);
                gbm_bo_destroy(bo);
                for (int plane_tmp = 0; plane_tmp < plane; plane_tmp++) {
                    close(buffer->fd[plane_tmp]);
                }

                destroyPipeWireSourceBuffer(buffer);
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
            gbm_bo_destroy(bo);
            for (int plane = 0; plane < buffer->planeCount; plane++) {
                close(buffer->fd[plane]);
            }

            destroyPipeWireSourceBuffer(buffer);
            return nullptr;
        }
    }

    return buffer;
}

void AbstractPipeWireStream::destroyPipeWireSourceBuffer(PipeWireSourceBuffer *buffer)
{
    if (buffer->buffer) {
        wl_buffer_destroy(buffer->buffer);
    }
    for (int plane = 0; plane < buffer->planeCount; plane++) {
        close(buffer->fd[plane]);
    }

    delete buffer;
}

bool AbstractPipeWireStream::hasDrmFourcc(uint32_t format)
{
    if (format == DRM_FORMAT_INVALID) {
        return false;
    }
    if (!m_avoidDMAbufs) {
        foreach(struct DRMFormatModifierPair *fm_pair, std::as_const(m_currentConstraints.dmabuf_format_modifier_pairs)) {
            if (fm_pair->fourcc == format) {
                return true;
            }
        }
    }
    return false;
}
