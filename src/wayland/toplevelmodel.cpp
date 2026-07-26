// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "toplevelmodel.h"
#include "amhelper.h"

#include <QApplication>

#include <utility>

ToplevelListModel::ToplevelListModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_foreignToplevelList(createForeignToplevelList())
{
    m_foreignToplevelListActive = m_foreignToplevelList->isActive();
    if (m_foreignToplevelListActive) {
        initConnection();
    } else {
        connect(m_foreignToplevelList.data(), &ForeignToplevelList::activeChanged, this, [this]{
            m_foreignToplevelListActive = m_foreignToplevelList->isActive();
            if (m_foreignToplevelListActive)
                initConnection();
        });
    }
}

ToplevelListModel::~ToplevelListModel()
{
    if (m_foreignToplevelList) {
        m_foreignToplevelList->requestStop();
    }

    // Handles must be destroyed before the list proxy. A selected entry may
    // keep both alive until its capture stream is torn down.
    m_selectedToplevels.clear();
    m_toplevels.clear();
    m_foreignToplevelList.clear();
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

    const ToplevelInfoPtr &toplevel = m_toplevels.at(index.row());
    switch (role) {
    case IconRole:
        return toplevel->iconPath;
    case NameRole:
        return toplevel->appName;
    case TitleRole:
        return toplevel->windowTitle;
    case SelectedRole:
        return m_selectedToplevels.contains(toplevel);
    }

    return QVariant();
}

bool ToplevelListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid)
        || index.row() >= m_toplevels.size()) {
        return false;
    }

    switch (role) {
    case IconRole:
        m_toplevels[index.row()]->iconPath = value.toString();
        break;
    case NameRole:
        m_toplevels[index.row()]->appName = value.toString();
        break;
    case TitleRole:
        m_toplevels[index.row()]->windowTitle = value.toString();
        break;
    case SelectedRole: {
        const ToplevelInfoPtr &toplevel = m_toplevels.at(index.row());
        const bool selected = value.toBool();
        if (m_selectedToplevels.contains(toplevel) == selected) {
            return false;
        }
        if (selected) {
            m_selectedToplevels.append(toplevel);
        } else {
            m_selectedToplevels.removeOne(toplevel);
        }
        Q_EMIT dataChanged(index, index, {SelectedRole});
        Q_EMIT hasSelectionChanged();
        return true;
    }
    default:
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
    roles[SelectedRole] = "sourceSelected";

    return roles;
}

QList<ToplevelInfoPtr> ToplevelListModel::selectedToplevels() const
{
    QList<ToplevelInfoPtr> result;
    result.reserve(m_selectedToplevels.size());
    for (const ToplevelInfoPtr &toplevel : m_toplevels) {
        if (m_selectedToplevels.contains(toplevel)) {
            result.append(toplevel);
        }
    }
    return result;
}

ToplevelInfoPtr ToplevelListModel::toplevelAt(int row) const
{
    if (row < 0 || row >= m_toplevels.count())
        return {};

    return m_toplevels[row];
}

bool ToplevelListModel::hasSelection() const
{
    return !m_selectedToplevels.isEmpty();
}

void ToplevelListModel::selectSingle(int row)
{
    if (row < 0 || row >= m_toplevels.size()) {
        return;
    }

    const ToplevelInfoPtr &toplevel = m_toplevels.at(row);
    if (m_selectedToplevels.size() == 1
        && m_selectedToplevels.constFirst() == toplevel) {
        return;
    }

    const bool hadSelection = hasSelection();
    m_selectedToplevels.clear();
    m_selectedToplevels.append(toplevel);
    Q_EMIT dataChanged(index(0),
                       index(m_toplevels.size() - 1),
                       {SelectedRole});
    if (hadSelection != hasSelection()) {
        Q_EMIT hasSelectionChanged();
    }
}

void ToplevelListModel::toggleSelection(int row)
{
    if (row < 0 || row >= m_toplevels.size()) {
        return;
    }

    const QModelIndex modelIndex = index(row);
    setData(modelIndex,
            !m_selectedToplevels.contains(m_toplevels.at(row)),
            SelectedRole);
}

void ToplevelListModel::removeToplevelAt(int row)
{
    if (row < 0 || row >= m_toplevels.size()) {
        return;
    }

    const bool hadSelection = hasSelection();
    m_selectedToplevels.removeOne(m_toplevels.at(row));
    beginRemoveRows(QModelIndex(), row, row);
    m_toplevels.removeAt(row);
    endRemoveRows();
    if (hadSelection != hasSelection()) {
        Q_EMIT hasSelectionChanged();
    }
}

void ToplevelListModel::initConnection()
{
    connect(m_foreignToplevelList.data(), &ForeignToplevelList::toplevelAdded,
            this, &ToplevelListModel::handleToplevelAdded);
    connect(m_foreignToplevelList.data(), &ForeignToplevelList::finished,
            this, &ToplevelListModel::handleFinished);
}

void ToplevelListModel::handleToplevelAdded(ForeignToplevelHandle *toplevel)
{
    const ToplevelInfoPtr info =
            ToplevelInfoPtr::create(toplevel, m_foreignToplevelList);
    connect(info->handle, &ForeignToplevelHandle::closed,
            this, &ToplevelListModel::handleToplevelClosed);
    connect(info->handle, &ForeignToplevelHandle::done,
            this, &ToplevelListModel::handleToplevelDone);
    connect(info->handle, &ForeignToplevelHandle::titleChanged,
            this, &ToplevelListModel::handleToplevelTitleChanged);
    connect(info->handle, &ForeignToplevelHandle::appIdChanged,
            this, &ToplevelListModel::handleToplevelAppIdChanged);
    connect(info->handle, &ForeignToplevelHandle::identifierChanged,
            this, &ToplevelListModel::handleIdentifierChanged);
    int count = m_toplevels.count();
    beginInsertRows(QModelIndex(), count, count);
    m_toplevels << info;
    endInsertRows();
}

void ToplevelListModel::handleFinished()
{
    if (m_toplevels.isEmpty()) {
        return;
    }

    const bool selectionChanged = hasSelection();
    m_selectedToplevels.clear();
    beginResetModel();
    m_toplevels.clear();
    endResetModel();
    if (selectionChanged) {
        Q_EMIT hasSelectionChanged();
    }
}

void ToplevelListModel::handleToplevelClosed()
{
    auto handle = static_cast<ForeignToplevelHandle *>(sender());
    for (int i = 0; i < m_toplevels.count(); i++) {
        auto toplevel = m_toplevels[i];
        if (toplevel->handle == handle) {
            removeToplevelAt(i);
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
                removeToplevelAt(i);
            }
            return;
        }
    }
}

void ToplevelListModel::handleToplevelTitleChanged(const QString &title)
{
    auto handle = static_cast<ForeignToplevelHandle *>(sender());
    for (const ToplevelInfoPtr &info : std::as_const(m_toplevels)) {
        if (info->handle == handle) {
            info->windowTitle = title;
            return;
        }
    }
}

void ToplevelListModel::handleToplevelAppIdChanged(const QString &appId)
{
    auto handle = static_cast<ForeignToplevelHandle *>(sender());
    for (const ToplevelInfoPtr &info : std::as_const(m_toplevels)) {
        if (info->handle == handle) {
            info->appID = appId;
            return;
        }
    }
}

void ToplevelListModel::handleIdentifierChanged(const QString &identifier)
{
    auto handle = static_cast<ForeignToplevelHandle *>(sender());
    for (const ToplevelInfoPtr &info : std::as_const(m_toplevels)) {
        if (info->handle == handle) {
            info->identifier = identifier;
            return;
        }
    }
}

ToplevelInfo::ToplevelInfo(ForeignToplevelHandle *toplevel,
                           const ForeignToplevelListPtr &list)
    : owner(list)
    , handle(toplevel)
{
}

ToplevelInfo::~ToplevelInfo()
{
    if (!handle) {
        return;
    }

    if (handle->isInitialized()) {
        handle->destroy();
    }
    handle->deleteLater();
    handle = nullptr;
}
