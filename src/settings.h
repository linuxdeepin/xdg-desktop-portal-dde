// Copyright © 2018 Red Hat, Inc
// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef XDG_DESKTOP_PORTAL_KDE_SETTINGS_H
#define XDG_DESKTOP_PORTAL_KDE_SETTINGS_H

#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>

#include <KConfigCore/KConfig>
#include <KConfigCore/KSharedConfig>

class SettingsPortal : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Settings")
public:
    /**
    * An identifier for change signals.
    * @note Copied from KGlobalSettings
    */
    enum ChangeType { PaletteChanged = 0, FontChanged, StyleChanged,
                      SettingsChanged, IconChanged, CursorChanged,
                      ToolbarStyleChanged, ClipboardConfigChanged,
                      BlockShortcuts, NaturalSortingChanged
                    };

    /**
    * Valid values for the settingsChanged signal
    * @note Copied from KGlobalSettings
    */
    enum SettingsCategory { SETTINGS_MOUSE, SETTINGS_COMPLETION, SETTINGS_PATHS,
                            SETTINGS_POPUPMENU, SETTINGS_QT, SETTINGS_SHORTCUTS,
                            SETTINGS_LOCALE, SETTINGS_STYLE
                          };

    explicit SettingsPortal(QObject *parent);
    ~SettingsPortal();

    typedef QMap<QString, QMap<QString, QVariant> > VariantMapMap;

    uint version() const { return 1; }

public Q_SLOTS:
    void ReadAll(const QStringList &groups);
    QDBusVariant Read(const QString &group, const QString &keys);

Q_SIGNALS:
    void SettingChanged(const QString &group, const QString &key, const QDBusVariant &value);

private Q_SLOTS:
    void fontChanged();
    void globalSettingChanged(int type, int arg);
    void toolbarStyleChanged();
private:
    KSharedConfigPtr m_kdeglobals;
};

#endif // XDG_DESKTOP_PORTAL_KDE_SETTINGS_H


