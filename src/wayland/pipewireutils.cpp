// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "pipewireutils.h"
#include "loggings.h"

#include <drm_fourcc.h>
#include <fcntl.h>

namespace PipeWireutils {

spa_video_format pipewireFormatStripAlpha(spa_video_format format)
{
    switch (format) {
    case SPA_VIDEO_FORMAT_BGRA:
        return SPA_VIDEO_FORMAT_BGRx;
    case SPA_VIDEO_FORMAT_ABGR:
        return SPA_VIDEO_FORMAT_xBGR;
    case SPA_VIDEO_FORMAT_RGBA:
        return SPA_VIDEO_FORMAT_RGBx;
    case SPA_VIDEO_FORMAT_ARGB:
        return SPA_VIDEO_FORMAT_xRGB;
    case SPA_VIDEO_FORMAT_ARGB_210LE:
        return SPA_VIDEO_FORMAT_xRGB_210LE;
    case SPA_VIDEO_FORMAT_ABGR_210LE:
        return SPA_VIDEO_FORMAT_xBGR_210LE;
    case SPA_VIDEO_FORMAT_RGBA_102LE:
        return SPA_VIDEO_FORMAT_RGBx_102LE;
    case SPA_VIDEO_FORMAT_BGRA_102LE:
        return SPA_VIDEO_FORMAT_BGRx_102LE;
    default:
        return SPA_VIDEO_FORMAT_UNKNOWN;
    }
}

uint32_t drmFormatfromWLShmFormat(wl_shm_format format)
{
    switch (format) {
    case WL_SHM_FORMAT_ARGB8888:
        return DRM_FORMAT_ARGB8888;
    case WL_SHM_FORMAT_XRGB8888:
        return DRM_FORMAT_XRGB8888;
    default:
        return (uint32_t)format;
    }
}

int32_t wlOutputTransformFromQscreen(const QScreen *screen)
{
    bool isPortrait = screen->size().height() > screen->size().width();
    switch (screen->orientation()) {
    case Qt::PortraitOrientation:
        return isPortrait ? WL_OUTPUT_TRANSFORM_NORMAL : WL_OUTPUT_TRANSFORM_90;
    case Qt::LandscapeOrientation:
        return isPortrait ? WL_OUTPUT_TRANSFORM_270 : WL_OUTPUT_TRANSFORM_NORMAL;
    case Qt::InvertedPortraitOrientation:
        return isPortrait ? WL_OUTPUT_TRANSFORM_180 : WL_OUTPUT_TRANSFORM_270;
    case Qt::InvertedLandscapeOrientation:
        return isPortrait ? WL_OUTPUT_TRANSFORM_90 : WL_OUTPUT_TRANSFORM_180;
    default:
        return -1;
    }
}

spa_video_format pipewireFormatFromDRMFormat(uint32_t format)
{
    switch (format) {
    case DRM_FORMAT_ARGB8888:
        return SPA_VIDEO_FORMAT_BGRA;
    case DRM_FORMAT_XRGB8888:
        return SPA_VIDEO_FORMAT_BGRx;
    case DRM_FORMAT_RGBA8888:
        return SPA_VIDEO_FORMAT_ABGR;
    case DRM_FORMAT_RGBX8888:
        return SPA_VIDEO_FORMAT_xBGR;
    case DRM_FORMAT_ABGR8888:
        return SPA_VIDEO_FORMAT_RGBA;
    case DRM_FORMAT_XBGR8888:
        return SPA_VIDEO_FORMAT_RGBx;
    case DRM_FORMAT_BGRA8888:
        return SPA_VIDEO_FORMAT_ARGB;
    case DRM_FORMAT_BGRX8888:
        return SPA_VIDEO_FORMAT_xRGB;
    case DRM_FORMAT_NV12:
        return SPA_VIDEO_FORMAT_NV12;
    case DRM_FORMAT_XRGB2101010:
        return SPA_VIDEO_FORMAT_xRGB_210LE;
    case DRM_FORMAT_XBGR2101010:
        return SPA_VIDEO_FORMAT_xBGR_210LE;
    case DRM_FORMAT_RGBX1010102:
        return SPA_VIDEO_FORMAT_RGBx_102LE;
    case DRM_FORMAT_BGRX1010102:
        return SPA_VIDEO_FORMAT_BGRx_102LE;
    case DRM_FORMAT_ARGB2101010:
        return SPA_VIDEO_FORMAT_ARGB_210LE;
    case DRM_FORMAT_ABGR2101010:
        return SPA_VIDEO_FORMAT_ABGR_210LE;
    case DRM_FORMAT_RGBA1010102:
        return SPA_VIDEO_FORMAT_RGBA_102LE;
    case DRM_FORMAT_BGRA1010102:
        return SPA_VIDEO_FORMAT_BGRA_102LE;
    case DRM_FORMAT_BGR888:
        return SPA_VIDEO_FORMAT_RGB;
    case DRM_FORMAT_RGB888:
        return SPA_VIDEO_FORMAT_BGR;
    default:
        qCCritical(SCREENCAST, "failed to convert drm format 0x%08x to spa_video_format", format);
        abort();
    }
}

wl_shm_format wlShmFormatFromDRMFormat(uint32_t format)
{
    switch (format) {
    case DRM_FORMAT_ARGB8888:
        return WL_SHM_FORMAT_ARGB8888;
    case DRM_FORMAT_XRGB8888:
        return WL_SHM_FORMAT_XRGB8888;
    default:
        return (enum wl_shm_format)format;
    }
}

}
