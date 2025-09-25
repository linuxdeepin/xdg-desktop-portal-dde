// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

ListView {
    id: view

    readonly property real itemHeight: 35

    highlightFollowsCurrentItem: true
    clip: true
    ButtonGroup { id: doubleExclusiveGroup }
    delegate: ColumnLayout {
        width: view.width
        CheckDelegate {
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: screenName
            ButtonGroup.group: doubleExclusiveGroup
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
