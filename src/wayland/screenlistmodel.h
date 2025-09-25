// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QAbstractListModel>
#include <QScreen>
#include <QGuiApplication>
#include <QtQmlIntegration>

class ScreenListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
public:
    enum ScreenRoles {
        NameRole = Qt::UserRole + 1,
    };

    explicit ScreenListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    QHash<int, QByteArray> roleNames() const override;
    QList<QPointer<QScreen>> selectedOutputs(int index);
    QScreen *outputAt(int row);

Q_SIGNALS:
    void hasSelectionChanged();

private Q_SLOTS:
    void onScreenAdded(QScreen *screen);
    void onScreenRemoved(QScreen *screen);

private:
    void refreshScreens();

private:
    QList<QScreen*> m_screens;
};
