// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef APPCHOOSERMODEL_H
#define APPCHOOSERMODEL_H
#include <QAbstractListModel>

class AppChooserModel : public QAbstractListModel
{
    Q_OBJECT
public:
    struct DesktopInfo {
      QString desktop;
      QString name;
      QString icon;
      bool selected;
    };

    enum Role{
        DataRole,
        NameRole,
        IconRole,
        SelectRole
    };
    explicit AppChooserModel(QObject *parent = nullptr);

    void click(const QModelIndex &index);
    void click(const QString &desktop);

    QStringList choices();

private:
    void loadApplications();

protected:
    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;

private:
    QList<DesktopInfo> m_datas;
};

#endif // APPCHOOSERMODEL_H
