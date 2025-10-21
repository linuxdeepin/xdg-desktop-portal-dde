// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "abstractpipewirestream.h"
#include "protocols/imagecapturesource.h"

#include <QScreen>

class OutputPipeWireStream : public AbstractPipeWireStream
{
    Q_OBJECT
public:
    OutputPipeWireStream(QPointer<ScreenCastContext> context,
                         PortalCommon::CursorModes mode,
                         QScreen *output,
                         QObject *parent = nullptr);

    int startScreencast() override;
    void startframeCapture() override;

private:
    QScreen *m_output = nullptr;
};
