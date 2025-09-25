// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "toplevelmodel.h"
#include "amhelper.h"

#include <QApplication>

ToplevelListModel::ToplevelListModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_foreignToplevelList(new ForeignToplevelList(this))
{
    m_foreignToplevelListActive = m_foreignToplevelList->isActive();
    if (m_foreignToplevelListActive) {
        initConnection();
    } else {
        connect(m_foreignToplevelList, &ForeignToplevelList::activeChanged, this, [this]{
            m_foreignToplevelListActive = m_foreignToplevelList->isActive();
            if (m_foreignToplevelListActive)
                initConnection();
        });
    }
}

int ToplevelListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return m_toplevels.size();
}

QVariant ToplevelListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_toplevels.size()) {
        return QVariant();
    }

    ToplevelInfo *toplevel = m_toplevels.at(index.row());
    switch (role) {
    case IconRole:
        return toplevel->iconPath;
    case NameRole:
        return toplevel->appName;
    case TitleRole:
        return toplevel->windowTitle;
    }

    return QVariant();
}

bool ToplevelListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid)
        || index.row() >= m_toplevels.size()
        || (role != IconRole && role != NameRole && role != TitleRole)) {
        return false;
    }

    switch (role) {
    case IconRole:
        m_toplevels[index.row()]->iconPath = value.toString();
        return false;
    case NameRole:
        m_toplevels[index.row()]->appName = value.toString();
        return false;
    case TitleRole:
        m_toplevels[index.row()]->windowTitle = value.toString();
        return false;
    }

    Q_EMIT dataChanged(index, index, {role});
    return true;
}

QHash<int, QByteArray> ToplevelListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IconRole] = "appIcon";
    roles[NameRole] = "name";
    roles[TitleRole] = "title";

    return roles;
}

QList<ToplevelInfo *> ToplevelListModel::selectedToplevels(int index)
{
    QList<ToplevelInfo *> ret;
    if (index > -1) {
        ret << m_toplevels[index];
    }
    return ret;
}

ToplevelInfo *ToplevelListModel::toplevelAt(int row)
{
    if (row < 0 || row >= m_toplevels.count())
        return nullptr;

    return m_toplevels[row];
}

void ToplevelListModel::initConnection()
{
    connect(m_foreignToplevelList, &ForeignToplevelList::toplevelAdded,
            this, &ToplevelListModel::handleToplevelAdded);
    connect(m_foreignToplevelList, &ForeignToplevelList::finished,
            this, &ToplevelListModel::handleFinished);
}

void ToplevelListModel::handleToplevelAdded(ForeignToplevelHandle *toplevel)
{
    auto info = new ToplevelInfo(toplevel);
    connect(info->handle, &ForeignToplevelHandle::closed,
            this, &ToplevelListModel::handleToplevelClosed);
    connect(info->handle, &ForeignToplevelHandle::done,
            this, &ToplevelListModel::handleToplevelDone);
    connect(info->handle, &ForeignToplevelHandle::titleChanged,
            this, &ToplevelListModel::handleToplevelTitleChanged);
    connect(info->handle, &ForeignToplevelHandle::appIdChanged,
            this, &ToplevelListModel::handleToplevelAppIdChanged);
    int count = m_toplevels.count();
    beginInsertRows(QModelIndex(), count, count);
    m_toplevels << info;
    endInsertRows();
}

void ToplevelListModel::handleFinished()
{
    foreach (ToplevelInfo *info, m_toplevels) {
        delete info;
    }
    m_toplevels.clear();
}

void ToplevelListModel::handleToplevelClosed()
{
    auto handle = static_cast<ForeignToplevelHandle *>(sender());
    for (int i = 0; i < m_toplevels.count(); i++) {
        auto toplevel = m_toplevels[i];
        if (toplevel->handle == handle) {
            beginRemoveRows(QModelIndex(), i, i);
            m_toplevels.removeOne(toplevel);
            delete toplevel;
            endRemoveRows();
            return;
        }
    }
}

void ToplevelListModel::handleToplevelDone()
{
    auto handle = static_cast<ForeignToplevelHandle *>(sender());
    for (int i = 0; i < m_toplevels.count(); i++) {
        if (m_toplevels[i]->handle == handle) {
            QModelIndex index = this->index(i);
            auto toplevel = m_toplevels[i];
            if (toplevel->appID != qApp->applicationName()) {
                AMHelpers::updateInfoFromAM(toplevel->appID, toplevel->appName, toplevel->iconPath);
                setData(index, toplevel->iconPath, IconRole);
                setData(index, toplevel->appName, NameRole);
                setData(index, toplevel->windowTitle, TitleRole);
            } else {
                beginRemoveRows(QModelIndex(), i, i);
                m_toplevels.removeOne(toplevel);
                delete toplevel;
                endRemoveRows();
            }
            return;
        }
    }
}

void ToplevelListModel::handleToplevelTitleChanged(const QString &title)
{
    auto handle = static_cast<ForeignToplevelHandle *>(sender());
    foreach (ToplevelInfo *info, m_toplevels) {
        if (info->handle == handle) {
            info->windowTitle = title;
            return;
        }
    }
}

void ToplevelListModel::handleToplevelAppIdChanged(const QString &appId)
{
    auto handle = static_cast<ForeignToplevelHandle *>(sender());
    foreach (ToplevelInfo *info, m_toplevels) {
        if (info->handle == handle) {
            info->appID = appId;
            return;
        }
    }
}

ToplevelInfo::ToplevelInfo(ForeignToplevelHandle *toplevel)
{
    handle = toplevel;
}

ToplevelInfo::~ToplevelInfo()
{
    handle->destroy();
    delete handle;
}
