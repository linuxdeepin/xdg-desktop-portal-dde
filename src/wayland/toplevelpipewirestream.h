// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "abstractpipewirestream.h"
#include "protocols/imagecapturesource.h"
#include "toplevelmodel.h"

#include <QScreen>

class ToplevelPipeWireStream : public AbstractPipeWireStream
{
    Q_OBJECT
public:
    ToplevelPipeWireStream(QPointer<ScreenCastContext> context,
                         PortalCommon::CursorModes mode,
                         ToplevelInfo *toplevel,
                         QObject *parent = nullptr);

    int startScreencast() override;
    void startframeCapture() override;

private Q_SLOTS:
    void handleToplevelClosed();

private:
    ToplevelInfo *m_toplevel = nullptr;
};
