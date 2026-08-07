// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: LGPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import screencast 1.0
import org.deepin.dtk 1.0 as D

D.DialogWindow {
    id: root
    readonly property int dialogWidth: 840
    readonly property int dialogHeight: screenSharingEnabled ? 680 : 360

    width: dialogWidth
    height: dialogHeight
    // Do not bind the window constraints to height. QQuickWindow may clamp
    // height when the constraints change, which creates a binding loop.
    minimumWidth: dialogWidth
    minimumHeight: dialogHeight
    maximumWidth: dialogWidth
    maximumHeight: dialogHeight
    modality: Qt.WindowModal

    property string clientAppName
    property bool keyboardRequested: false
    property bool pointerRequested: false
    property bool screenSharingEnabled: false
    property bool allowScreens: true
    property bool allowWindows: true
    property alias keyboardEnabled: keyboardCheckBox.checked
    property alias pointerEnabled: pointerCheckBox.checked
    property alias allowRestore: restoreCheckBox.checked
    property alias viewLayoutIndex: viewLayout.currentIndex
    property alias outputIndex: screensView.currentIndex
    property alias toplevelIndex: toplevelsView.currentIndex
    property var outputsModel: screensView.model
    property var toplevelsModel: toplevelsView.model

    signal accept()
    signal reject()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 10

        Label {
            text: qsTr("Application [%1] requests remote desktop access").arg(root.clientAppName)
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
        }
        Label {
            text: qsTr("Choose what the application may control and share")
            Layout.alignment: Qt.AlignHCenter
        }

        GroupBox {
            title: qsTr("Remote input")
            visible: root.keyboardRequested || root.pointerRequested
            Layout.fillWidth: true
            RowLayout {
                anchors.fill: parent
                CheckBox {
                    id: keyboardCheckBox
                    visible: root.keyboardRequested
                    checked: root.keyboardRequested
                    text: qsTr("Keyboard")
                }
                CheckBox {
                    id: pointerCheckBox
                    visible: root.pointerRequested
                    checked: root.pointerRequested
                    text: qsTr("Mouse and touchpad")
                }
            }
        }

        Label {
            visible: root.screenSharingEnabled
            text: qsTr("Select the screen or window to share")
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
        }

        RowLayout {
            visible: root.screenSharingEnabled && root.allowScreens && root.allowWindows
            Layout.alignment: Qt.AlignHCenter
            Button {
                text: qsTr("Screen")
                highlighted: viewLayout.currentIndex === 0
                flat: !highlighted
                onClicked: viewLayout.currentIndex = 0
            }
            Button {
                text: qsTr("Window")
                highlighted: viewLayout.currentIndex === 1
                flat: !highlighted
                onClicked: viewLayout.currentIndex = 1
            }
        }

        StackLayout {
            id: viewLayout
            visible: root.screenSharingEnabled
            Layout.fillWidth: true
            Layout.preferredHeight: 360
            currentIndex: root.allowScreens ? 0 : 1

            Background {
                OutputListView {
                    id: screensView
                    anchors.fill: parent
                    rightMargin: 50
                    model: ScreenListModel {}
                    itemHeight: 36
                    currentIndex: -1
                }
            }
            Background {
                ToplevelList {
                    id: toplevelsView
                    anchors.fill: parent
                    rightMargin: 50
                    model: ToplevelListModel {}
                    itemHeight: 36
                    currentIndex: -1
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            CheckBox {
                id: restoreCheckBox
                checked: true
                text: qsTr("Allow restoring on future sessions")
            }
            Item { Layout.fillWidth: true }
            Button {
                text: qsTr("Reject")
                onClicked: root.reject()
            }
            D.RecommandButton {
                text: qsTr("Allow")
                enabled: (root.keyboardEnabled || root.pointerEnabled)
                         || (root.screenSharingEnabled
                             && ((root.viewLayoutIndex === 0 && root.outputIndex >= 0)
                                 || (root.viewLayoutIndex === 1 && root.toplevelIndex >= 0)))
                onClicked: root.accept()
            }
        }
    }
}
