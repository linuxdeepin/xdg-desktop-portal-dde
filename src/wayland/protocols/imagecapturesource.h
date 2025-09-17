// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <private/qwaylandclientextension_p.h>
#include <qwayland-ext-image-capture-source-v1.h>

class OutputImageCaptureSourceManager
    : public QWaylandClientExtensionTemplate<OutputImageCaptureSourceManager>
    , public QtWayland::ext_output_image_capture_source_manager_v1
{
    Q_OBJECT
public:
    OutputImageCaptureSourceManager(QObject *parent = nullptr);
    uint32_t version();
};

class ForeignToplevelImageCaptureSourceManager
    : public QWaylandClientExtensionTemplate<ForeignToplevelImageCaptureSourceManager>
    , public QtWayland::ext_foreign_toplevel_image_capture_source_manager_v1
{
    Q_OBJECT
public:
    ForeignToplevelImageCaptureSourceManager(QObject *parent = nullptr);
    uint32_t version();
};
