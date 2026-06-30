// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "remotedesktopchooser.h"
#include "amhelper.h"
#include "loggings.h"
#include "screenlistmodel.h"
#include "toplevelmodel.h"

#include <QQmlApplicationEngine>
#include <QQuickWindow>

RemoteDesktopChooser::RemoteDesktopChooser(const QString &appId,
                                           PortalCommon::DeviceTypes requestedDevices,
                                           bool screenSharingEnabled,
                                           PortalCommon::SourceTypes sourceTypes,
                                           QObject *parent)
    : QObject(parent)
    , m_engine(new QQmlApplicationEngine(this))
{
    connect(m_engine, &QQmlApplicationEngine::objectCreationFailed, this, [] {
        qCCritical(REMOTEDESKTOP) << "Failed to create the remote desktop chooser";
    });
    m_engine->load(QUrl(QStringLiteral("qrc:/screencast/RemoteDesktopChooserWindow.qml")));
    if (m_engine->rootObjects().isEmpty())
        return;

    m_window = qobject_cast<QQuickWindow *>(m_engine->rootObjects().constFirst());
    if (!m_window)
        return;

    connect(m_window, SIGNAL(accept()), this, SLOT(accept()));
    connect(m_window, SIGNAL(reject()), this, SLOT(reject()));
    connect(m_window, &QQuickWindow::closing, this, &RemoteDesktopChooser::reject);
    m_window->setProperty("clientAppName", AMHelpers::nameFromAM(appId));
    m_window->setProperty("keyboardRequested", requestedDevices.testFlag(PortalCommon::Keyboard));
    m_window->setProperty("pointerRequested", requestedDevices.testFlag(PortalCommon::Pointer));
    m_window->setProperty("screenSharingEnabled", screenSharingEnabled);
    m_window->setProperty("allowScreens", sourceTypes.testFlag(PortalCommon::Monitor));
    m_window->setProperty("allowWindows", sourceTypes.testFlag(PortalCommon::Window));
}

RemoteDesktopChooser::~RemoteDesktopChooser()
{
    if (m_window)
        m_window->close();
}

void RemoteDesktopChooser::showWindow()
{
    if (m_window) {
        m_window->show();
        m_window->raise();
        m_window->requestActivate();
    }
}

QWindow *RemoteDesktopChooser::windowHandle() const
{
    return m_window;
}

PortalCommon::DeviceTypes RemoteDesktopChooser::selectedDevices() const
{
    PortalCommon::DeviceTypes devices = PortalCommon::None;
    if (m_window && m_window->property("keyboardEnabled").toBool())
        devices |= PortalCommon::Keyboard;
    if (m_window && m_window->property("pointerEnabled").toBool())
        devices |= PortalCommon::Pointer;
    return devices;
}

QList<QPointer<QScreen>> RemoteDesktopChooser::selectedOutputs() const
{
    if (!m_window || !m_window->property("screenSharingEnabled").toBool()
        || m_window->property("viewLayoutIndex").toInt() != 0)
        return {};
    auto *model = qobject_cast<ScreenListModel *>(m_window->property("outputsModel").value<QObject *>());
    return model ? model->selectedOutputs(m_window->property("outputIndex").toInt()) : QList<QPointer<QScreen>>{};
}

QList<ToplevelInfo *> RemoteDesktopChooser::selectedToplevels() const
{
    if (!m_window || !m_window->property("screenSharingEnabled").toBool()
        || m_window->property("viewLayoutIndex").toInt() != 1)
        return {};
    auto *model = qobject_cast<ToplevelListModel *>(m_window->property("toplevelsModel").value<QObject *>());
    return model ? model->selectedToplevels(m_window->property("toplevelIndex").toInt()) : QList<ToplevelInfo *>{};
}

bool RemoteDesktopChooser::allowRestore() const
{
    return m_window && m_window->property("allowRestore").toBool();
}

void RemoteDesktopChooser::accept()
{
    if (m_finished)
        return;
    m_finished = true;
    Q_EMIT accepted();
    Q_EMIT finished(Accepted);
    deleteLater();
}

void RemoteDesktopChooser::reject()
{
    if (m_finished)
        return;
    m_finished = true;
    Q_EMIT rejected();
    Q_EMIT finished(Rejected);
    deleteLater();
}
