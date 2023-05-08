// Copyright © 2016-2018 Red Hat, Inc
// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef XDG_DESKTOP_PORTAL_KDE_FILECHOOSER_H
#define XDG_DESKTOP_PORTAL_KDE_FILECHOOSER_H

#include <QDBusObjectPath>
#include <QMetaType>
#include <QDBusAbstractAdaptor>
#include <QDialog>

class KFileWidget;
class QDialogButtonBox;

class FileDialog : public QDialog
{
    Q_OBJECT
public:
    friend class FileChooserPortal;

    FileDialog(QDialog *parent = nullptr, Qt::WindowFlags flags = {});
    ~FileDialog();

private:
    QDialogButtonBox *m_buttons;
protected:
    KFileWidget *m_fileWidget;
};

class FileChooserPortal : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.FileChooser")
public:
    // Keep in sync with qflatpakfiledialog from flatpak-platform-plugin
    typedef struct {
        uint type;
        QString filterString;
    } Filter;
    typedef QList<Filter> Filters;

    typedef struct {
        QString userVisibleName;
        Filters filters;
    } FilterList;
    typedef QList<FilterList> FilterListList;

    explicit FileChooserPortal(QObject *parent);
    ~FileChooserPortal();

public Q_SLOTS:
    uint OpenFile(const QDBusObjectPath &handle,
                  const QString &app_id,
                  const QString &parent_window,
                  const QString &title,
                  const QVariantMap &options,
                  QVariantMap &results);

    uint SaveFile(const QDBusObjectPath &handle,
                  const QString &app_id,
                  const QString &parent_window,
                  const QString &title,
                  const QVariantMap &options,
                  QVariantMap &results);
};

#endif // XDG_DESKTOP_PORTAL_KDE_FILECHOOSER_H
