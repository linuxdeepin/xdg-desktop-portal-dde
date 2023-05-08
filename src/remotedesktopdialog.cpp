// Copyright © 2018 Red Hat, Inc
// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "remotedesktopdialog.h"
#include "ui_remotedesktopdialog.h"

#include <QLoggingCategory>
#include <QStandardPaths>
#include <QSettings>
#include <QPushButton>
#include "klocalizedstring.h"

Q_LOGGING_CATEGORY(XdgDesktopPortalDdeRemoteDesktopDialog, "xdp-dde-remote-desktop-dialog")

RemoteDesktopDialog::RemoteDesktopDialog(const QString &appName, RemoteDesktopPortal::DeviceTypes deviceTypes, bool screenSharingEnabled,
                                         bool multiple, QDialog *parent, Qt::WindowFlags flags)
    : QDialog(parent, flags)
    , m_dialog(new Ui::RemoteDesktopDialog)
{
    m_dialog->setupUi(this);

    m_dialog->screenCastWidget->setVisible(screenSharingEnabled);
    if (screenSharingEnabled) {
        connect(m_dialog->screenCastWidget, &QListWidget::itemDoubleClicked, this, &RemoteDesktopDialog::accept);

        if (multiple) {
            m_dialog->screenCastWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
        }
    }


    m_dialog->screenCastWidget->itemAt(0, 0)->setSelected(true);

    m_dialog->keyboardCheckbox->setChecked(deviceTypes.testFlag(RemoteDesktopPortal::Keyboard));
    m_dialog->pointerCheckbox->setChecked(deviceTypes.testFlag(RemoteDesktopPortal::Pointer));
    m_dialog->touchScreenCheckbox->setChecked(deviceTypes.testFlag(RemoteDesktopPortal::TouchScreen));

    connect(m_dialog->buttonBox, &QDialogButtonBox::accepted, this, &RemoteDesktopDialog::accept);
    connect(m_dialog->buttonBox, &QDialogButtonBox::rejected, this, &RemoteDesktopDialog::reject);

    m_dialog->buttonBox->button(QDialogButtonBox::Ok)->setText(i18n("Share"));
    m_dialog->buttonBox->button(QDialogButtonBox::Cancel)->setText(i18n("Cancel"));

    QString applicationName;
    const QString desktopFile = appName + QLatin1String(".desktop");
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
        setWindowTitle(i18n("Select what to share with the requesting application"));
    } else {
        setWindowTitle(i18n("Select what to share with %1", applicationName));
    }
}

RemoteDesktopDialog::~RemoteDesktopDialog()
{
    delete m_dialog;
}

QList<quint32> RemoteDesktopDialog::selectedScreens() const
{
    return m_dialog->screenCastWidget->selectedScreens();
}

RemoteDesktopPortal::DeviceTypes RemoteDesktopDialog::deviceTypes() const
{
    RemoteDesktopPortal::DeviceTypes types = RemoteDesktopPortal::None;
    if (m_dialog->keyboardCheckbox->isChecked())
        types |= RemoteDesktopPortal::Keyboard;
    if (m_dialog->pointerCheckbox->isChecked())
        types |= RemoteDesktopPortal::Pointer;
    if (m_dialog->touchScreenCheckbox->isChecked())
        types |= RemoteDesktopPortal::TouchScreen;

    return types;
}
