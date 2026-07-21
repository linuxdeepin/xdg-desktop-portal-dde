// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <private/qguiapplication_p.h>
#include <private/qwaylanddisplay_p.h>
#include <private/qwaylandintegration_p.h>
#include <QPointer>

#include <cstdint>
#include <limits>

inline QtWaylandClient::QWaylandIntegration *waylandIntegration()
{
    return dynamic_cast<QtWaylandClient::QWaylandIntegration *>(QGuiApplicationPrivate::platformIntegration());
}

inline QPointer<QtWaylandClient::QWaylandDisplay> waylandDisplay()
{
    return waylandIntegration()->display();
}

inline bool isSafeShmBufferLayout(uint32_t width, uint32_t height, uint32_t stride)
{
    if (width == 0 || height == 0) {
        return false;
    }

    constexpr uint64_t maxInt = std::numeric_limits<int>::max();
    if (width > maxInt || height > maxInt) {
        return false;
    }

    const uint64_t expectedStride = uint64_t(width) * 4;
    const uint64_t byteCount = expectedStride * height;
    return expectedStride == stride && byteCount <= maxInt;
}
