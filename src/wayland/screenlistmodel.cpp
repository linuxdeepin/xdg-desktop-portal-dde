// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "screenlistmodel.h"

#include <QPointer>

ScreenListModel::ScreenListModel(QObject *parent)
    : QAbstractListModel(parent)
{
    refreshScreens();
    connect(qApp, &QGuiApplication::screenAdded, this, &ScreenListModel::onScreenAdded);
    connect(qApp, &QGuiApplication::screenRemoved, this, &ScreenListModel::onScreenRemoved);
}

void ScreenListModel::refreshScreens()
{
    beginResetModel();
    m_screens = QGuiApplication::screens();
    endResetModel();
}

int ScreenListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return m_screens.size();
}

QVariant ScreenListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_screens.size()) {
        return QVariant();
    }

    QScreen *screen = m_screens.at(index.row());
    switch (role) {
    case NameRole:
        return screen->name();
    }

    return QVariant();
}

bool ScreenListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid) || role != Qt::CheckStateRole) {
        return false;
    }

    if (index.data(Qt::CheckStateRole) == value) {
        return true;
    }

    Q_EMIT dataChanged(index, index, {role});
    return true;
}

QHash<int, QByteArray> ScreenListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[NameRole] = "screenName";
    return roles;
}

QList<QPointer<QScreen>> ScreenListModel::selectedOutputs(int index)
{
    QList<QPointer<QScreen>> ret;
    if (index > -1) {
        ret << m_screens[index];
    }
    return ret;
}

QScreen *ScreenListModel::outputAt(int row)
{
    return m_screens[row];
}

void ScreenListModel::onScreenAdded(QScreen *screen)
{
    Q_UNUSED(screen)
    refreshScreens();
}

void ScreenListModel::onScreenRemoved(QScreen *screen)
{
    Q_UNUSED(screen)
    refreshScreens();
}
