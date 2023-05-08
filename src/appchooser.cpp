// Copyright © 2016-2018 Red Hat, Inc
// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "appchooser.h"
#include "appchooserdialog.h"
#include "utils.h"

#include <QLoggingCategory>

#include <KLocalizedString>

Q_LOGGING_CATEGORY(XdgDesktopPortalKdeAppChooser, "xdp-kde-app-chooser")

AppChooserPortal::AppChooserPortal(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
}

AppChooserPortal::~AppChooserPortal()
{
}

uint AppChooserPortal::ChooseApplication(const QDBusObjectPath &handle,
                                         const QString &app_id,
                                         const QString &parent_window,
                                         const QStringList &choices,
                                         const QVariantMap &options,
                                         QVariantMap &results)
{
    qCDebug(XdgDesktopPortalKdeAppChooser) << "ChooseApplication called with parameters:";
    qCDebug(XdgDesktopPortalKdeAppChooser) << "    handle: " << handle.path();
    qCDebug(XdgDesktopPortalKdeAppChooser) << "    app_id: " << app_id;
    qCDebug(XdgDesktopPortalKdeAppChooser) << "    parent_window: " << parent_window;
    qCDebug(XdgDesktopPortalKdeAppChooser) << "    choices: " << choices;
    qCDebug(XdgDesktopPortalKdeAppChooser) << "    options: " << options;

    QString latestChoice;

    if (options.contains(QStringLiteral("last_choice"))) {
        latestChoice = options.value(QStringLiteral("last_choice")).toString();
    }

    AppChooserDialog *appDialog = new AppChooserDialog(choices, latestChoice, options.value(QStringLiteral("filename")).toString());
    m_appChooserDialogs.insert(handle.path(), appDialog);
    Utils::setParentWindow(appDialog, parent_window);

    int result = appDialog->exec();

    if (result) {
        results.insert(QStringLiteral("choice"), appDialog->selectedApplication());
    }

    m_appChooserDialogs.remove(handle.path());
    appDialog->deleteLater();

    return !result;
}

void AppChooserPortal::UpdateChoices(const QDBusObjectPath &handle, const QStringList &choices)
{
    qCDebug(XdgDesktopPortalKdeAppChooser) << "UpdateChoices called with parameters:";
    qCDebug(XdgDesktopPortalKdeAppChooser) << "    handle: " << handle.path();
    qCDebug(XdgDesktopPortalKdeAppChooser) << "    choices: " << choices;

    if (m_appChooserDialogs.contains(handle.path())) {
        m_appChooserDialogs.value(handle.path())->updateChoices(choices);
    }
}
