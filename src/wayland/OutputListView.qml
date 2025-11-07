// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

ListView {
    id: view

    property real itemHeight

    highlightFollowsCurrentItem: true
    clip: true
    ButtonGroup { id: doubleExclusiveGroup }
    delegate: CheckDelegate {
        width: view.width
        height: view.itemHeight
        text: screenName
        ButtonGroup.group: doubleExclusiveGroup
        onClicked: view.currentIndex = index

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
