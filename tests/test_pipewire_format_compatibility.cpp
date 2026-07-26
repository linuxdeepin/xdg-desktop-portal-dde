// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "wayland/pipewireutils.h"

#include <array>
#include <iostream>
#include <utility>

namespace {

using FormatPair = std::pair<spa_video_format, spa_video_format>;

bool expect(bool condition, const char *message)
{
    if (condition) {
        return true;
    }

    std::cerr << message << '\n';
    return false;
}

} // namespace

int main()
{
    constexpr std::array<FormatPair, 8> alphaFormats{{
            {SPA_VIDEO_FORMAT_BGRA, SPA_VIDEO_FORMAT_BGRx},
            {SPA_VIDEO_FORMAT_ABGR, SPA_VIDEO_FORMAT_xBGR},
            {SPA_VIDEO_FORMAT_RGBA, SPA_VIDEO_FORMAT_RGBx},
            {SPA_VIDEO_FORMAT_ARGB, SPA_VIDEO_FORMAT_xRGB},
            {SPA_VIDEO_FORMAT_ARGB_210LE, SPA_VIDEO_FORMAT_xRGB_210LE},
            {SPA_VIDEO_FORMAT_ABGR_210LE, SPA_VIDEO_FORMAT_xBGR_210LE},
            {SPA_VIDEO_FORMAT_RGBA_102LE, SPA_VIDEO_FORMAT_RGBx_102LE},
            {SPA_VIDEO_FORMAT_BGRA_102LE, SPA_VIDEO_FORMAT_BGRx_102LE},
    }};

    for (const auto &[withAlpha, withoutAlpha] : alphaFormats) {
        if (!expect(PipeWireutils::pipewireFormatStripAlpha(withAlpha)
                            == withoutAlpha,
                    "An alpha SHM format did not map to its byte-compatible X format")
            || !expect(PipeWireutils::pipewireFormatStripAlpha(withoutAlpha)
                               == SPA_VIDEO_FORMAT_UNKNOWN,
                       "An X format was incorrectly promoted to a format with alpha")) {
            return 1;
        }
    }

    return 0;
}
