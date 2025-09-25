// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QObject>
#include <QtQmlIntegration>

class PortalCommon : public QObject
{
    Q_OBJECT
    QML_ELEMENT
public:
    enum SourceType {
        Any = 0,
        Monitor = 1,
        Window = 2,
    };
    Q_ENUM(SourceType)
    Q_DECLARE_FLAGS(SourceTypes, SourceType)

    enum CursorModes {
        Hidden = 1,
        Embedded = 2,
        Metadata = 4,
    };
    Q_ENUM(CursorModes)

    enum PersistMode {
        NoPersist = 0,
        PersistWhileRunning = 1,
        PersistUntilRevoked = 2,
    };
    Q_ENUM(PersistMode)

    enum BufferType {
        SHM = 0,
        DMABUF = 1,
    };
    Q_ENUM(BufferType)

    enum DeviceType {
        None = 0x0,
        Keyboard = 0x1,
        Pointer = 0x2,
        TouchScreen = 0x4,
        All = (Keyboard | Pointer | TouchScreen),
    };
    Q_DECLARE_FLAGS(DeviceTypes, DeviceType)
};
