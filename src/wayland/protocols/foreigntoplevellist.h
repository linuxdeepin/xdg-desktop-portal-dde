// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <private/qwaylandclientextension_p.h>
#include <qwayland-ext-foreign-toplevel-list-v1.h>

class ForeignToplevelHandle;

class ForeignToplevelList
    : public QWaylandClientExtensionTemplate<ForeignToplevelList>
    , public QtWayland::ext_foreign_toplevel_list_v1
{
    Q_OBJECT
public:
    ForeignToplevelList(QObject *parent = nullptr);
    uint32_t version();

Q_SIGNALS:
    void toplevelAdded(ForeignToplevelHandle *handle);
    void finished();

protected:
    void ext_foreign_toplevel_list_v1_toplevel(struct ::ext_foreign_toplevel_handle_v1 *toplevel) override;
    void ext_foreign_toplevel_list_v1_finished() override;
};

class ForeignToplevelHandle : public QObject, public QtWayland::ext_foreign_toplevel_handle_v1
{
    Q_OBJECT
public:
    ForeignToplevelHandle(struct ::ext_foreign_toplevel_handle_v1 *object, QObject *parent = nullptr);

Q_SIGNALS:
    void closed();
    void done();
    void titleChanged(const QString &title);
    void appIdChanged(const QString &app_id);
    void identifierChanged(const QString &identifier);

protected:
    void ext_foreign_toplevel_handle_v1_closed() override;
    void ext_foreign_toplevel_handle_v1_done() override;
    void ext_foreign_toplevel_handle_v1_title(const QString &title) override;
    void ext_foreign_toplevel_handle_v1_app_id(const QString &app_id) override;
    void ext_foreign_toplevel_handle_v1_identifier(const QString &identifier) override;
};
