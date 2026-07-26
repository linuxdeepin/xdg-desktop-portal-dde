// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "protocols/foreigntoplevellist.h"

#include <QAbstractListModel>
#include <QSharedPointer>
#include <QtQmlIntegration>

using QStringMap = QMap<QString, QString>;
Q_DECLARE_METATYPE(QStringMap)

struct ToplevelInfo {
    ToplevelInfo(ForeignToplevelHandle *toplevel,
                 const ForeignToplevelListPtr &owner);
    ~ToplevelInfo();
    ForeignToplevelListPtr owner;
    ForeignToplevelHandle *handle = nullptr;
    QString appID;
    QString windowTitle;

    QString iconPath;
    QString appName;
    QString identifier;
};

using ToplevelInfoPtr = QSharedPointer<ToplevelInfo>;

class ToplevelListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY hasSelectionChanged)
public:
    enum ScreenRoles {
        IconRole = Qt::UserRole + 1,
        NameRole,
        TitleRole,
        SelectedRole,
    };

    explicit ToplevelListModel(QObject *parent = nullptr);
    ~ToplevelListModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    QHash<int, QByteArray> roleNames() const override;
    QList<ToplevelInfoPtr> selectedToplevels() const;
    ToplevelInfoPtr toplevelAt(int row) const;
    bool hasSelection() const;
    Q_INVOKABLE void selectSingle(int row);
    Q_INVOKABLE void toggleSelection(int row);

Q_SIGNALS:
    void hasSelectionChanged();

private:
    void initConnection();
    void removeToplevelAt(int row);

private Q_SLOTS:
    void handleToplevelAdded(ForeignToplevelHandle *toplevel);
    void handleFinished();
    void handleToplevelClosed();
    void handleToplevelDone();
    void handleToplevelTitleChanged(const QString &title);
    void handleToplevelAppIdChanged(const QString &appId);
    void handleIdentifierChanged(const QString &identifier);

private:
    QList<ToplevelInfoPtr> m_toplevels;
    QList<ToplevelInfoPtr> m_selectedToplevels;
    ForeignToplevelListPtr m_foreignToplevelList;
    bool m_foreignToplevelListActive = false;

    friend class ToplevelListModelTestAccess;
};
