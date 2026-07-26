// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "wayland/screenlistmodel.h"
#include "wayland/toplevelmodel.h"

#include <QGuiApplication>

#include <iostream>

class ToplevelListModelTestAccess
{
public:
    static void append(ToplevelListModel &model,
                       const ToplevelInfoPtr &toplevel)
    {
        model.m_toplevels.append(toplevel);
    }

    static void remove(ToplevelListModel &model, int row)
    {
        model.removeToplevelAt(row);
    }
};

namespace {

bool expect(bool condition, const char *message)
{
    if (condition) {
        return true;
    }

    std::cerr << message << '\n';
    return false;
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    ScreenListModel model;
    if (!expect(model.rowCount() > 0,
                "The offscreen platform did not expose a test screen")
        || !expect(model.roleNames().value(ScreenListModel::SelectedRole)
                           == QByteArrayLiteral("sourceSelected"),
                   "The QML selection role has an unexpected name")) {
        return 1;
    }

    int selectionChanges = 0;
    QObject::connect(&model,
                     &ScreenListModel::hasSelectionChanged,
                     [&selectionChanges] {
        ++selectionChanges;
    });

    QScreen *firstScreen = model.outputAt(0);
    model.selectSingle(0);
    const QList<QPointer<QScreen>> selected = model.selectedOutputs();
    if (!expect(model.hasSelection(),
                "Selecting a screen did not update hasSelection")
        || !expect(selected.size() == 1 && selected.constFirst() == firstScreen,
                   "The selected screen was not returned in model order")
        || !expect(model.data(model.index(0), ScreenListModel::SelectedRole).toBool(),
                   "The selected role was not updated")
        || !expect(selectionChanges == 1,
                   "Selecting a screen emitted an unexpected number of notifications")) {
        return 1;
    }

    model.selectSingle(0);
    if (!expect(selectionChanges == 1,
                "Selecting the same sole screen emitted another notification")) {
        return 1;
    }

    model.toggleSelection(0);
    if (!expect(!model.hasSelection(),
                "Deselecting the screen did not clear hasSelection")
        || !expect(model.selectedOutputs().isEmpty(),
                   "The deselected screen remained in selectedOutputs")
        || !expect(selectionChanges == 2,
                   "Deselecting a screen emitted an unexpected number of notifications")) {
        return 1;
    }

    model.selectSingle(-1);
    model.selectSingle(model.rowCount());
    model.toggleSelection(-1);
    model.toggleSelection(model.rowCount());
    if (!expect(selectionChanges == 2,
                "Invalid rows changed the screen selection")) {
        return 1;
    }

    ToplevelListModel toplevelModel;
    const ToplevelInfoPtr firstToplevel =
            ToplevelInfoPtr::create(nullptr, ForeignToplevelListPtr{});
    const ToplevelInfoPtr selectedToplevel =
            ToplevelInfoPtr::create(nullptr, ForeignToplevelListPtr{});
    const ToplevelInfoPtr replacementToplevel =
            ToplevelInfoPtr::create(nullptr, ForeignToplevelListPtr{});
    ToplevelListModelTestAccess::append(toplevelModel, firstToplevel);
    ToplevelListModelTestAccess::append(toplevelModel, selectedToplevel);
    ToplevelListModelTestAccess::append(toplevelModel, replacementToplevel);

    int toplevelSelectionChanges = 0;
    QObject::connect(&toplevelModel,
                     &ToplevelListModel::hasSelectionChanged,
                     [&toplevelSelectionChanges] {
        ++toplevelSelectionChanges;
    });

    toplevelModel.selectSingle(1);
    if (!expect(toplevelModel.hasSelection(),
                "Selecting a window did not update hasSelection")
        || !expect(toplevelModel.selectedToplevels()
                           == QList<ToplevelInfoPtr>{selectedToplevel},
                   "The model did not retain the exact selected window")
        || !expect(toplevelModel.data(toplevelModel.index(1),
                                     ToplevelListModel::SelectedRole).toBool(),
                   "The selected window role was not updated")
        || !expect(toplevelSelectionChanges == 1,
                   "Selecting a window emitted an unexpected number of notifications")) {
        return 1;
    }

    ToplevelListModelTestAccess::remove(toplevelModel, 1);
    if (!expect(!toplevelModel.hasSelection(),
                "Removing the selected window did not clear hasSelection")
        || !expect(toplevelModel.selectedToplevels().isEmpty(),
                   "The removed window remained selected")
        || !expect(toplevelModel.toplevelAt(1) == replacementToplevel,
                   "The expected replacement row was not present")
        || !expect(!toplevelModel.data(toplevelModel.index(1),
                                      ToplevelListModel::SelectedRole).toBool(),
                   "A window that moved into the selected row inherited consent")
        || !expect(toplevelSelectionChanges == 2,
                   "Removing the selected window emitted unexpected notifications")) {
        return 1;
    }

    return 0;
}
