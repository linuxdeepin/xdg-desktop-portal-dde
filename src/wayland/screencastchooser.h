// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "portalcommon.h"
#include "toplevelmodel.h"

#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickWindow>
#include <QPointer>
#include <QRect>
#include <QScreen>

class ScreenCastChooser : public QObject
{
    Q_OBJECT
public:
    enum DialogResult {
        Accepted = 0,
        Rejected = 1
    };
    Q_ENUM(DialogResult)

    explicit ScreenCastChooser(const QString &appID,
                               PortalCommon::SourceTypes types,
                               bool multipleSources,
                               bool persistenceRequested,
                               QObject *parent = nullptr);
    ~ScreenCastChooser() override;
    void showWindow();
    void closeWindow();
    QRect selectedRegion() const;
    QList<QPointer<QScreen>> selectedOutputs() const;
    QList<ToplevelInfoPtr> selectedToplevels() const;
    bool allowRestore() const;
    QWindow *windowHandle() const;

Q_SIGNALS:
    void finished(DialogResult result);
    void accepted();
    void rejected();

public Q_SLOTS:
    void reject();
    void accept();
    void handleWindowClosed();

private:
    QQmlApplicationEngine *m_engine;
    QQuickWindow *m_window;
    PortalCommon::SourceTypes m_types;
    bool m_multipleSources = false;
    bool m_finished = false;
};
