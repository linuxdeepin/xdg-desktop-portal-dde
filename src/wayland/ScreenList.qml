// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

import QtQuick 2.15

import screencast 1.0

GridView {
    id: view

    readonly property real itemWidth: 210
    readonly property real itemHeight: 210
    readonly property real itemBorder: 5

    cellWidth: itemWidth
    cellHeight: itemHeight
    highlight: highlight
    highlightFollowsCurrentItem: false
    model: ScreenListModel {}
    currentIndex: -1
    clip: true
    delegate: Rectangle {
        color: "steelblue"
        width: itemWidth - 2 * itemBorder
        height: itemHeight - 2 * itemBorder

        Text {
            text: screenName
            font.bold: true
            anchors.centerIn: parent
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                view.currentIndex = index;
            }
        }
    }

    Component {
        id: highlight

        Rectangle {
            width: view.cellWidth; height: view.cellHeight
            color: "transparent"
            border.color: "red"
            border.width: itemBorder
            x: view.currentItem.x - itemBorder
            y: view.currentItem.y - itemBorder
            Behavior on x { SpringAnimation { spring: 3; damping: 0.2 } }
            Behavior on y { SpringAnimation { spring: 3; damping: 0.2 } }
        }
    }
}
