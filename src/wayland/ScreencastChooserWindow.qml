// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.5

Window {
    id: root

    width: 640
    height: 400
    visible: true
    // flags: Qt.Window | Qt.WindowCloseButtonHint
    title: qsTr("Choose Screencast source!")

    property alias allowRestore: allowRestoreItem.checked
    property alias outputIndex: screensView.currentIndex
    property var outputsModel: screensView.model
    readonly property real itemMargin: 20

    signal accept()
    signal reject()

    ColumnLayout {
        anchors.fill: parent
        TabBar {
            id: bar
            Layout.fillWidth: true
            Layout.fillHeight: true
            TabButton {
                text: qsTr("Outputs")
            }
        }

        StackLayout {
            width: parent.width
            currentIndex: bar.currentIndex
            ScreenList {
                id: screensView

                bottomMargin : root.itemMargin
                leftMargin : root.itemMargin
                rightMargin : root.itemMargin
                topMargin : root.itemMargin
                implicitWidth: parent.width
                implicitHeight: parent.width

                onCurrentIndexChanged: {
                    root.accept()
                }
            }
        }

        CheckBox {
            id: allowRestoreItem

            Layout.fillWidth: true
            Layout.preferredHeight: 40
            Layout.alignment: Qt.AlignLeft || Qt.AlignVCenter || Qt.AlignBottom
            checked: true
            text: qsTr("Allow restoring on future sessions")
        }
    }

    Item {
        anchors.fill: parent
        Keys.onPressed: (event)=> {
                            if (event.key === Qt.Key_Escape) {
                                root.reject();
                            }
                        }
    }
}
