// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import org.deepin.dtk 1.0 as D

ListView {
    id: view

    readonly property real itemHeight: 40
    readonly property real iconSize: 64

    clip: true
    spacing: 10
    highlightFollowsCurrentItem: true
    ButtonGroup { id: doubleExclusiveGroup }
    delegate: ColumnLayout {
        width: view.width
        D.CheckDelegate {
            Layout.fillWidth: true
            Layout.fillHeight: true
            ButtonGroup.group: doubleExclusiveGroup
            content: RowLayout {
                spacing: 10
                D.DciIcon {
                    width: view.itemHeight
                    height: view.itemHeight
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
        }

        Background {
            readonly property real sideMargin: 10

            Layout.fillWidth: true
            Layout.leftMargin: sideMargin
            Layout.rightMargin: sideMargin
            Layout.preferredHeight: 1
        }
    }

    ScrollBar.vertical: ScrollBar {}
}
