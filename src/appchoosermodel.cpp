// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "appchoosermodel.h"
#include "iteminfo.h"

#include <QDBusInterface>
#include <QDBusConnection>
#include <QSettings>

AppChooserModel::AppChooserModel(QObject *parent)
    :QAbstractListModel(parent)
{
    QMetaObject::invokeMethod(this, &AppChooserModel::loadApplications, Qt::QueuedConnection);
}

void AppChooserModel::click(const QModelIndex &index)
{
    if (index.row() >= 0 && index.row() <= m_datas.size() -1) {
        beginResetModel();
        m_datas[index.row()].selected = ! m_datas[index.row()].selected;
        endResetModel();
    }
}

void AppChooserModel::click(const QString &desktop)
{
    for (auto data : m_datas) {
        if (data.desktop == desktop) {
            data.selected = true;
            break;
        }
    }
}

QStringList AppChooserModel::choices()
{
    QStringList list;
    for (auto data : m_datas) {
        if (data.selected)
            list.append(data.desktop);
    }
    return list;
}

void AppChooserModel::loadApplications()
{
    ItemInfo::registerMetaType();

#ifdef QT_DEBUG
    QDBusPendingCall call = QDBusInterface("com.deepin.dde.daemon.Launcher", "/com/deepin/dde/daemon/Launcher", "com.deepin.dde.daemon.Launcher", QDBusConnection::sessionBus()).asyncCall(QString("GetAllItemInfos"));
#else
    QDBusPendingCall call = QDBusInterface("org.deepin.dde.daemon.Launcher1", "/org/deepin/dde/daemon/Launcher1", "org.deepin.dde.daemon.Launcher1", QDBusConnection::sessionBus()).asyncCall(QString("GetAllItemInfos"));
#endif
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher* call){
        QDBusPendingReply<ItemInfoList> reply = *call;
        if (!reply.isError()) {
            m_datas.clear();
            const auto itemList = reply.value();
            for (auto item : itemList) {
                QSettings settings(item.m_desktop, QSettings::IniFormat);
                settings.beginGroup("Desktop Entry");
                if (!settings.contains("MimeType"))
                    continue;

                DesktopInfo info;
                info.desktop = item.m_desktop;
                info.name = item.m_name;
                info.icon = item.m_iconKey;
                info.selected = false;

                beginInsertRows(QModelIndex(), m_datas.size(), m_datas.size());
                m_datas.append(info);
                endInsertRows();
            }
        }
        call->deleteLater();
    });
}

int AppChooserModel::rowCount(const QModelIndex &parent) const
{
    return m_datas.size();
}

QVariant AppChooserModel::data(const QModelIndex &index, int role) const
{
    const DesktopInfo &info = m_datas.at(index.row());
    switch (role) {
    case AppChooserModel::DataRole:
        return info.desktop;
    case AppChooserModel::IconRole:
        return info.icon;
    case AppChooserModel::NameRole:
        return info.name;
    case AppChooserModel::SelectRole:
        return info.selected;
    default:
        return QVariant();
    }
}
