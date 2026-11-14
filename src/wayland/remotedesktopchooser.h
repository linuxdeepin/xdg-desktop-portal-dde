// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "portalcommon.h"

#include <QObject>
#include <QPointer>
#include <QRect>
#include <QScreen>

class QQmlApplicationEngine;
class QQuickWindow;
class ToplevelInfo;

class RemoteDesktopChooser : public QObject
{
    Q_OBJECT
public:
    enum DialogResult {
        Accepted = 0,
        Rejected = 1,
    };
    Q_ENUM(DialogResult)

    RemoteDesktopChooser(const QString &appId,
                         PortalCommon::DeviceTypes requestedDevices,
                         bool screenSharingEnabled,
                         PortalCommon::SourceTypes sourceTypes,
                         QObject *parent = nullptr);
    ~RemoteDesktopChooser() override;

    void showWindow();
    QWindow *windowHandle() const;
    PortalCommon::DeviceTypes selectedDevices() const;
    QList<QPointer<QScreen>> selectedOutputs() const;
    QList<ToplevelInfo *> selectedToplevels() const;
    bool allowRestore() const;

Q_SIGNALS:
    void finished(DialogResult result);
    void accepted();
    void rejected();

public Q_SLOTS:
    void accept();
    void reject();

private:
    QQmlApplicationEngine *m_engine = nullptr;
    QQuickWindow *m_window = nullptr;
    bool m_finished = false;
};
