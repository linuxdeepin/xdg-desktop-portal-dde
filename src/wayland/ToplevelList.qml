// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import org.deepin.dtk 1.0 as D

D.ListView {
    id: view

    property real itemHeight
    property bool multipleSelection: false
    readonly property bool hasSelection: Boolean(view.model && view.model.hasSelection)
    readonly property real iconSize: 16

    clip: true
    highlightFollowsCurrentItem: true
    delegate: D.CheckDelegate {
        width: view.width
        height: view.itemHeight
        checkable: false
        checked: sourceSelected
        content: RowLayout {
            spacing: 10
            D.DciIcon {
                width: view.iconSize
                height: view.iconSize
                name: appIcon
                sourceSize: Qt.size(width, height)
            }
            Label {
                text: name
            }
            Label {
                text: title
                opacity: 0.7
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }
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
