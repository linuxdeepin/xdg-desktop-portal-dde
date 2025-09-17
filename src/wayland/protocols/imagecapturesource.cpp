// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "imagecapturesource.h"

OutputImageCaptureSourceManager::OutputImageCaptureSourceManager(QObject *parent)
    : QWaylandClientExtensionTemplate<OutputImageCaptureSourceManager>(1)
    , QtWayland::ext_output_image_capture_source_manager_v1()
{
}

uint32_t OutputImageCaptureSourceManager::version()
{
    return QtWayland::ext_output_image_capture_source_manager_v1::version();
}

ForeignToplevelImageCaptureSourceManager::ForeignToplevelImageCaptureSourceManager(QObject *parent)
    : QWaylandClientExtensionTemplate<ForeignToplevelImageCaptureSourceManager>(1)
    , QtWayland::ext_foreign_toplevel_image_capture_source_manager_v1()
{
}

uint32_t ForeignToplevelImageCaptureSourceManager::version()
{
    return QtWayland::ext_foreign_toplevel_image_capture_source_manager_v1::version();
}
