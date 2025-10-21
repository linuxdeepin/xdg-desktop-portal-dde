// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "protocols/foreigntoplevellist.h"

#include <QAbstractListModel>
#include <QtQmlIntegration>

using QStringMap = QMap<QString, QString>;
Q_DECLARE_METATYPE(QStringMap)

struct ToplevelInfo {
    ToplevelInfo(ForeignToplevelHandle *toplevel);
    ~ToplevelInfo();
    ForeignToplevelHandle *handle = nullptr;
    QString appID;
    QString windowTitle;

    QString iconPath;
    QString appName;
    QString identifier;
};

class ToplevelListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
public:
    enum ScreenRoles {
        IconRole = Qt::UserRole + 1,
        NameRole,
        TitleRole,
    };

    explicit ToplevelListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    QHash<int, QByteArray> roleNames() const override;
    QList<ToplevelInfo *> selectedToplevels(int index);
    ToplevelInfo *toplevelAt(int row);

private:
    void initConnection();

private Q_SLOTS:
    void handleToplevelAdded(ForeignToplevelHandle *toplevel);
    void handleFinished();
    void handleToplevelClosed();
    void handleToplevelDone();
    void handleToplevelTitleChanged(const QString &title);
    void handleToplevelAppIdChanged(const QString &appId);
    void handleIdentifierChanged(const QString &identifier);

private:
    QList<ToplevelInfo*> m_toplevels;
    ForeignToplevelList *m_foreignToplevelList = nullptr;
    bool m_foreignToplevelListActive = false;
};
