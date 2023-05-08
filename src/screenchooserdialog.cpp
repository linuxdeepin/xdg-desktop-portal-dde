// Copyright © 2018 Red Hat, Inc
// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "screenchooserdialog.h"
#include "ui_screenchooserdialog.h"

#include <KLocalizedString>
#include <QPushButton>
#include <QStandardPaths>
#include <QSettings>

ScreenChooserDialog::ScreenChooserDialog(const QString &appName, bool multiple, QDialog *parent, Qt::WindowFlags flags)
    : QDialog(parent, flags)
    , m_dialog(new Ui::ScreenChooserDialog)
{
    m_dialog->setupUi(this);

    if (multiple) {
        m_dialog->screenView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    }

    connect(m_dialog->buttonBox, &QDialogButtonBox::accepted, this, &ScreenChooserDialog::accept);
    connect(m_dialog->buttonBox, &QDialogButtonBox::rejected, this, &ScreenChooserDialog::reject);
    connect(m_dialog->screenView, &QListWidget::itemDoubleClicked, this, &ScreenChooserDialog::accept);

    m_dialog->buttonBox->button(QDialogButtonBox::Ok)->setText(i18n("Share"));

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
        setWindowTitle(i18n("Select screen to share with the requesting application"));
    } else {
        setWindowTitle(i18n("Select screen to share with %1").arg(applicationName));
    }
}

ScreenChooserDialog::~ScreenChooserDialog()
{
    delete m_dialog;
}

QList<quint32> ScreenChooserDialog::selectedScreens() const
{
    return m_dialog->screenView->selectedScreens();
}
