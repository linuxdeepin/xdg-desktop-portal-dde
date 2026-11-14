// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import org.deepin.dtk 1.0 as D

D.ListView {
    id: view

    property real itemHeight
    property bool multipleSelection: false
    readonly property bool hasSelection: Boolean(view.model && view.model.hasSelection)

    highlightFollowsCurrentItem: true
    clip: true
    delegate: D.CheckDelegate {
        width: view.width
        height: view.itemHeight
        text: screenName
        checkable: false
        checked: sourceSelected
        onClicked: {
            view.currentIndex = index
            if (view.multipleSelection) {
                view.model.toggleSelection(index)
            } else {
                view.model.selectSingle(index)
            }
        }

        Background {
            readonly property real sideMargin: 10

            anchors {
                bottom: parent.bottom
                left: parent.left
                right: parent.right
                leftMargin: sideMargin
                rightMargin: sideMargin
            }
            width: parent.width
            height: 1
        }
    }

    ScrollBar.vertical: ScrollBar {}
}
