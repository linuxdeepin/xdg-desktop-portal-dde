// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import screencast 1.0

import org.deepin.dtk 1.0 as D

D.DialogWindow {
    id: root

    width: 840
    height: 584
    minimumWidth: width
    minimumHeight: height
    maximumWidth: width
    maximumHeight: height
    modality: Qt.WindowModal

    property alias allowRestore: restoreCheckBox.checked
    property alias viewLayoutIndex: viewLayout.currentIndex
    property var outputsModel: screensView.model
    property var toplevelsModel: toplevelsView.model
    property string clientAppName
    property bool allowMonitor: true
    property bool allowWindow: true
    property bool multipleSources: false
    property bool persistenceRequested: false
    readonly property bool selectionValid: {
        const monitorAllowed = root.allowMonitor
        const windowAllowed = root.allowWindow
        const monitorSelected = monitorAllowed && screensView.hasSelection
        const windowSelected = windowAllowed && toplevelsView.hasSelection
        if (root.multipleSources) {
            return monitorSelected || windowSelected
        }

        const currentView = viewLayout.currentIndex

        return (currentView === 0 && monitorSelected) ||
               (currentView === 1 && windowSelected)
    }
    readonly property real itemMargin: 10
    readonly property real scrollBarMargin: 50

    signal accept()
    signal reject()

    Component.onCompleted: {
        // ScreenListModel is populated synchronously. Match the established
        // portal chooser behaviour by preselecting the sole permitted output.
        if (!root.multipleSources &&
                root.allowMonitor && !root.allowWindow &&
                screensView.count === 1) {
            screensView.currentIndex = 0
            screensView.model.selectSingle(0)
        }
    }

    ColumnLayout {
        spacing: 8
        width: parent.width

        Label {
            text: qsTr("Application [%1] requests to share you screen content").arg(clientAppName)
            font.bold: true
            Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        }

        Label {
            text: qsTr("Please select the screen or window you wish to share")
            Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
        }

        RowLayout {
            spacing: root.itemMargin
            Layout.alignment: Qt.AlignHCenter
            Button {
                text: qsTr("Screen")
                visible: root.allowMonitor
                highlighted: viewLayout.currentIndex === 0
                flat: !highlighted
                onClicked: viewLayout.currentIndex = 0
            }
            Button {
                text: qsTr("Window")
                visible: root.allowWindow
                highlighted: viewLayout.currentIndex === 1
                flat: !highlighted
                onClicked: viewLayout.currentIndex = 1
            }
        }

        StackLayout {
            id: viewLayout

            readonly property real viewMargin: 62
            readonly property real viewHeight: 372
            readonly property real radius: 6
            readonly property real delegateHeight: 36
            readonly property color darkColor: "black"
            readonly property color lightColor: "white"

            Layout.preferredWidth: parent.width
            Layout.preferredHeight: viewHeight
            currentIndex: root.allowMonitor ? 0 : 1
            Background {
                radius: parent.radius
                darkColor: parent.darkColor
                lightColor: parent.lightColor
                OutputListView {
                    id: screensView

                    anchors.fill: parent
                    rightMargin: root.scrollBarMargin
                    model: ScreenListModel {}
                    itemHeight: viewLayout.delegateHeight
                    multipleSelection: root.multipleSources
                    currentIndex: -1
                }
            }

            Background {
                radius: parent.radius
                darkColor: parent.darkColor
                lightColor: parent.lightColor
                ToplevelList {
                    id: toplevelsView

                    anchors.fill: parent
                    rightMargin: root.scrollBarMargin
                    model: ToplevelListModel {}
                    itemHeight: viewLayout.delegateHeight
                    multipleSelection: root.multipleSources
                    currentIndex: -1
                }
            }
        }

        RowLayout {
            spacing: root.itemMargin
            Item {
                Layout.fillWidth: true

                CheckBox {
                    id: restoreCheckBox

                    anchors.verticalCenter: parent.verticalCenter
                    visible: root.persistenceRequested
                    checked: root.persistenceRequested
                    text: qsTr("Allow restoring on future sessions")
                }
            }

            Row {
                Layout.alignment: Qt.AlignHCenter
                spacing: root.itemMargin
                Button {
                    id: acceptBtn

                    text: qsTr("Accept")
                    enabled: root.selectionValid
                    onClicked: root.accept();
                }
                D.RecommandButton {
                    text: qsTr("Reject")
                    onClicked: root.reject()
                }
            }

            Item {
                Layout.fillWidth: true
            }
        }
    }
}
