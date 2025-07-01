// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "screencastchooser.h"
#include "screenlistmodel.h"
#include "loggings.h"

#include <QSettings>
#include <QStandardPaths>

constexpr auto URI = "screencast";

ScreenCastChooser::ScreenCastChooser( const QString &appID, PortalCommon::SourceTypes types, QObject *parent)
    : QObject(parent)
    , m_engine(new QQmlApplicationEngine(this))
    , m_window(nullptr)
{
    qmlRegisterType<PortalCommon>(URI, 0, 1,"PortalCommon");
    qmlRegisterType<ScreenListModel>(URI, 0, 1,"ScreenListModel");

    QObject::connect(m_engine, &QQmlApplicationEngine::objectCreationFailed, this, [this](){
        qCCritical(SCREENCAST, "qml create failed!");
    });

    m_engine->load(QUrl("qrc:/screencast/ScreencastChooserWindow.qml"));
    QObject *rootObject =  m_engine->rootObjects().first();
    QQuickWindow* win = qobject_cast<QQuickWindow *>(rootObject);
    if (win) {
        connect(win, &QQuickWindow::closing, this, &ScreenCastChooser::handleWindowClosed);
        connect(win, SIGNAL(accept()), this, SLOT(accept()));
        connect(win, SIGNAL(reject()), this, SLOT(reject()));
        m_window = win;
    }
}

ScreenCastChooser::~ScreenCastChooser()
{
    closeWindow();
}

void ScreenCastChooser::showWindow()
{
    if (m_window) {
        m_window->show();
        m_window->raise();
        m_window->requestActivate();
    }
}

void ScreenCastChooser::closeWindow()
{
    if (m_window) {
        m_window->close();
    }
}

void ScreenCastChooser::handleWindowClosed()
{
    QQuickWindow* win = qobject_cast<QQuickWindow *>(sender());
    if (win) {
        win->deleteLater();
    }
}

QString ScreenCastChooser::applicationName(const QString &appId)
{
    QString applicationName;
    const QString desktopFile = appId + QStringLiteral(".desktop");
    const QStringList desktopFileLocations = QStandardPaths::locateAll(QStandardPaths::ApplicationsLocation, desktopFile, QStandardPaths::LocateFile);
    for (const QString &location : desktopFileLocations) {
        QSettings settings(location, QSettings::IniFormat);
        settings.beginGroup(QStringLiteral("Desktop Entry"));
        if (settings.contains(QStringLiteral("X-GNOME-FullName"))) {
            applicationName = settings.value(QStringLiteral("X-GNOME-FullName")).toString();
        } else {
            applicationName = settings.value(QStringLiteral("Name")).toString();
        }

        if (!applicationName.isEmpty()) {
            break;
        }
    }

    if (applicationName.isEmpty()) {
        applicationName = appId;
    }

    return applicationName;
}

QRect ScreenCastChooser::selectedRegion() const
{
    // TODO
    return QRect();
}

QList<QPointer<QScreen>> ScreenCastChooser::selectedOutputs() const
{
    ScreenListModel *model = dynamic_cast<ScreenListModel *>
            (m_window->property("outputsModel").value<QObject *>());
    if (!model) {
        return {};
    }
    return model->selectedOutputs(m_window->property("outputIndex").toInt());
}

bool ScreenCastChooser::allowRestore() const
{
    return m_window->property("allowRestore").toBool();
}

QWindow *ScreenCastChooser::windowHandle() const
{
    if (m_window) {
        return m_window;
    }

    return nullptr;
}

void ScreenCastChooser::reject()
{
    Q_EMIT rejected();
    Q_EMIT finished(ScreenCastChooser::Rejected);
    deleteLater();
}

void ScreenCastChooser::accept()
{
    Q_EMIT accepted();
    Q_EMIT finished(ScreenCastChooser::Accepted);
    deleteLater();
}
