// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QAbstractListModel>
#include <QPointer>
#include <QScreen>
#include <QSet>
#include <QGuiApplication>
#include <QtQmlIntegration>

class ScreenListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY hasSelectionChanged)
public:
    enum ScreenRoles {
        NameRole = Qt::UserRole + 1,
        SelectedRole,
    };

    explicit ScreenListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    QHash<int, QByteArray> roleNames() const override;
    QList<QPointer<QScreen>> selectedOutputs() const;
    QScreen *outputAt(int row) const;
    bool hasSelection() const;
    Q_INVOKABLE void selectSingle(int row);
    Q_INVOKABLE void toggleSelection(int row);

Q_SIGNALS:
    void hasSelectionChanged();

private Q_SLOTS:
    void onScreenAdded(QScreen *screen);
    void onScreenRemoved(QScreen *screen);

private:
    void refreshScreens();

private:
    QList<QScreen*> m_screens;
    QSet<QScreen *> m_selectedScreens;
};
