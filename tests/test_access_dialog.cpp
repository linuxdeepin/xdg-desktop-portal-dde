// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "accessdialog.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>

#include <iostream>

namespace
{

bool clickButton(AccessDialog &dialog, const QString &text)
{
    const auto buttons = dialog.findChildren<QAbstractButton *>();
    for (auto button : buttons) {
        if (button->text() == text) {
            button->click();
            return true;
        }
    }
    return false;
}

bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

}

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    const QVariantMap options{
        {QStringLiteral("grant_label"), QStringLiteral("TEST_ALLOW")},
        {QStringLiteral("deny_label"), QStringLiteral("TEST_DENY")},
        {QStringLiteral("modal"), true},
    };

    AccessDialog allowDialog(QStringLiteral("Title"),
                             QStringLiteral("Subtitle"),
                             QStringLiteral("Body"),
                             options,
                             {});
    if (!expect(allowDialog.windowModality() == Qt::WindowModal,
                "Modal access dialog is not window-modal")
        || !expect(clickButton(allowDialog, QStringLiteral("TEST_ALLOW")),
                "Could not find the allow button")
        || !expect(allowDialog.result() == QDialog::Accepted,
                   "Allow button did not map to QDialog::Accepted")) {
        return 1;
    }

    AccessDialog denyDialog(QStringLiteral("Title"),
                            QStringLiteral("Subtitle"),
                            QStringLiteral("Body"),
                            options,
                            {});
    if (!expect(clickButton(denyDialog, QStringLiteral("TEST_DENY")),
                "Could not find the deny button")
        || !expect(denyDialog.result() == QDialog::Rejected,
                   "Deny button did not map to QDialog::Rejected")) {
        return 1;
    }

    const AccessDialogTypes::OptionList choices{
        {QStringLiteral("remember"), QStringLiteral("Remember"), {}, QStringLiteral("false")},
        {QStringLiteral("quality"),
         QStringLiteral("Quality"),
         {{QStringLiteral("low"), QStringLiteral("Low")},
          {QStringLiteral("high"), QStringLiteral("High")}},
         QStringLiteral("low")},
    };
    AccessDialog choicesDialog(QStringLiteral("Title"), {}, {}, options, choices);
    auto checkBox = choicesDialog.findChild<QCheckBox *>();
    auto comboBox = choicesDialog.findChild<QComboBox *>();
    if (!expect(checkBox && comboBox, "Access choices controls were not created")) {
        return 1;
    }
    checkBox->setChecked(true);
    comboBox->setCurrentIndex(1);
    const auto selected = choicesDialog.selectedChoices();
    return expect(selected.size() == 2, "Access choices result has wrong size")
            && expect(selected.at(0).value == QStringLiteral("true"),
                      "Boolean access choice was not updated")
            && expect(selected.at(1).value == QStringLiteral("high"),
                      "Enumerated access choice was not updated")
            ? 0
            : 1;
}
