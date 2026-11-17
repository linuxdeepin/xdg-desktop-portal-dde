// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "screencastchooser.h"
#include "screenlistmodel.h"
#include "toplevelmodel.h"
#include "loggings.h"
#include "amhelper.h"

#include <QSettings>
#include <QStandardPaths>
#include <QVariantMap>

constexpr auto URI = "screencast";

ScreenCastChooser::ScreenCastChooser(const QString &appID,
                                     PortalCommon::SourceTypes types,
                                     bool multipleSources,
                                     bool persistenceRequested,
                                     QObject *parent)
    : QObject(parent)
    , m_engine(new QQmlApplicationEngine(this))
    , m_window(nullptr)
    , m_types(types)
    , m_multipleSources(multipleSources)
{
    QObject::connect(m_engine, &QQmlApplicationEngine::objectCreationFailed, this, [this](){
        qCCritical(SCREENCAST, "qml create failed!");
    });

    QVariantMap initialProperties;
    initialProperties.insert(QStringLiteral("clientAppName"), appID);
    initialProperties.insert(QStringLiteral("allowMonitor"),
                             m_types.testFlag(PortalCommon::Monitor));
    initialProperties.insert(QStringLiteral("allowWindow"),
                             m_types.testFlag(PortalCommon::Window));
    initialProperties.insert(QStringLiteral("multipleSources"), m_multipleSources);
    initialProperties.insert(QStringLiteral("persistenceRequested"), persistenceRequested);
    m_engine->setInitialProperties(initialProperties);
    m_engine->load(QUrl("qrc:/screencast/ScreencastChooserWindow.qml"));
    if (m_engine->rootObjects().isEmpty()) {
        qCCritical(SCREENCAST) << "xdpw: screencast chooser has no root object";
        return;
    }

    QObject *rootObject = m_engine->rootObjects().first();
    QQuickWindow* win = qobject_cast<QQuickWindow *>(rootObject);

    if (win) {
        connect(win, &QQuickWindow::closing, this, &ScreenCastChooser::handleWindowClosed);
        connect(win, SIGNAL(accept()), this, SLOT(accept()));
        connect(win, SIGNAL(reject()), this, SLOT(reject()));
        m_window = win;

        AMHelpers::nameFromAMAsync(appID, this, [this](QString resolvedName) {
            if (m_window && !resolvedName.isEmpty()) {
                m_window->setProperty("clientAppName", resolvedName);
            }
        });
    }
}

ScreenCastChooser::~ScreenCastChooser()
{
    m_finished = true;
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
        if (m_window == win) {
            m_window = nullptr;
        }
        win->deleteLater();
    }
    if (!m_finished) {
        reject();
    }
}

QRect ScreenCastChooser::selectedRegion() const
{
    // TODO
    return QRect();
}

QList<QPointer<QScreen>> ScreenCastChooser::selectedOutputs() const
{
    if (!m_window
        || !m_types.testFlag(PortalCommon::Monitor)) {
        return {};
    }

    ScreenListModel *model = dynamic_cast<ScreenListModel *>
            (m_window->property("outputsModel").value<QObject *>());
    if (!model) {
        return {};
    }
    if (!m_multipleSources
        && m_window->property("viewLayoutIndex").toInt() != 0) {
        return {};
    }
    return model->selectedOutputs();
}

QList<ToplevelInfoPtr> ScreenCastChooser::selectedToplevels() const
{
    if (!m_window
        || !m_types.testFlag(PortalCommon::Window)) {
        return {};
    }

    ToplevelListModel *model = dynamic_cast<ToplevelListModel *>
            (m_window->property("toplevelsModel").value<QObject *>());
    if (!model) {
        return {};
    }
    if (!m_multipleSources
        && m_window->property("viewLayoutIndex").toInt() != 1) {
        return {};
    }
    return model->selectedToplevels();
}

bool ScreenCastChooser::allowRestore() const
{
    return m_window && m_window->property("allowRestore").toBool();
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
    if (m_finished) {
        return;
    }

    m_finished = true;
    Q_EMIT rejected();
    Q_EMIT finished(ScreenCastChooser::Rejected);
    deleteLater();
}

void ScreenCastChooser::accept()
{
    if (m_finished || !m_window) {
        return;
    }

    const qsizetype outputCount = selectedOutputs().size();
    const qsizetype toplevelCount = selectedToplevels().size();
    const qsizetype sourceCount = outputCount + toplevelCount;
    if (sourceCount == 0
        || (!m_multipleSources && sourceCount != 1)) {
        qCWarning(SCREENCAST)
                << "xdpw: refusing source chooser acceptance with invalid source count"
                << sourceCount << "multiple" << m_multipleSources;
        return;
    }

    qCInfo(SCREENCAST) << "xdpw: source chooser accepted"
                       << "view" << m_window->property("viewLayoutIndex").toInt()
                       << "outputs" << outputCount
                       << "toplevels" << toplevelCount
                       << "multiple" << m_multipleSources;
    m_finished = true;
    Q_EMIT accepted();
    Q_EMIT finished(ScreenCastChooser::Accepted);
    deleteLater();
}
