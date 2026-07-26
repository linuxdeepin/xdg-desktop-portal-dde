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
    const bool hadSelection = hasSelection();
    const QList<QScreen *> screens = QGuiApplication::screens();
    for (auto iterator = m_selectedScreens.begin();
         iterator != m_selectedScreens.end();) {
        if (!screens.contains(*iterator)) {
            iterator = m_selectedScreens.erase(iterator);
        } else {
            ++iterator;
        }
    }

    beginResetModel();
    m_screens = screens;
    endResetModel();
    if (hadSelection != hasSelection()) {
        Q_EMIT hasSelectionChanged();
    }
}

int ScreenListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return m_screens.count();
}

QVariant ScreenListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_screens.count()) {
        return QVariant();
    }

    QScreen *screen = m_screens.at(index.row());
    switch (role) {
    case NameRole:
        return screen->name();
    case SelectedRole:
        return m_selectedScreens.contains(screen);
    }

    return QVariant();
}

bool ScreenListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid)
        || index.row() >= m_screens.count()
        || role != SelectedRole) {
        return false;
    }

    QScreen *screen = m_screens.at(index.row());
    const bool selected = value.toBool();
    if (m_selectedScreens.contains(screen) == selected) {
        return false;
    }

    if (selected) {
        m_selectedScreens.insert(screen);
    } else {
        m_selectedScreens.remove(screen);
    }
    Q_EMIT dataChanged(index, index, {SelectedRole});
    Q_EMIT hasSelectionChanged();
    return true;
}

QHash<int, QByteArray> ScreenListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[NameRole] = "screenName";
    roles[SelectedRole] = "sourceSelected";
    return roles;
}

QList<QPointer<QScreen>> ScreenListModel::selectedOutputs() const
{
    QList<QPointer<QScreen>> result;
    result.reserve(m_selectedScreens.size());
    for (QScreen *screen : m_screens) {
        if (m_selectedScreens.contains(screen)) {
            result.append(screen);
        }
    }
    return result;
}

QScreen *ScreenListModel::outputAt(int row) const
{
    return row >= 0 && row < m_screens.size()
            ? m_screens.at(row)
            : nullptr;
}

bool ScreenListModel::hasSelection() const
{
    return !m_selectedScreens.isEmpty();
}

void ScreenListModel::selectSingle(int row)
{
    if (row < 0 || row >= m_screens.size()) {
        return;
    }

    QScreen *screen = m_screens.at(row);
    if (m_selectedScreens.size() == 1
        && m_selectedScreens.contains(screen)) {
        return;
    }

    const bool hadSelection = hasSelection();
    m_selectedScreens.clear();
    m_selectedScreens.insert(screen);
    Q_EMIT dataChanged(index(0),
                       index(m_screens.size() - 1),
                       {SelectedRole});
    if (hadSelection != hasSelection()) {
        Q_EMIT hasSelectionChanged();
    }
}

void ScreenListModel::toggleSelection(int row)
{
    if (row < 0 || row >= m_screens.size()) {
        return;
    }

    const QModelIndex modelIndex = index(row);
    setData(modelIndex,
            !m_selectedScreens.contains(m_screens.at(row)),
            SelectedRole);
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
