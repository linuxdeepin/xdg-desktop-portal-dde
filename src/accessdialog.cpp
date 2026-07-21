// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "accessdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QIcon>
#include <QLabel>
#include <QVBoxLayout>

#include <utility>

AccessDialog::AccessDialog(const QString &title,
                           const QString &subtitle,
                           const QString &body,
                           const QVariantMap &options,
                           const AccessDialogTypes::OptionList &choices,
                           QWidget *parent)
    : DDialog(parent)
    , m_choices(choices)
{
    setAccessibleName(QStringLiteral("AccessDialog"));
    setTitle(title);
    setWordWrapTitle(true);
    setWindowModality(options.value(QStringLiteral("modal"), true).toBool()
                              ? Qt::WindowModal
                              : Qt::NonModal);
    setOnButtonClickedClose(false);
    setCloseButtonVisible(true);

    const QString iconName = options.value(QStringLiteral("icon")).toString();
    setIcon(QIcon::fromTheme(iconName.isEmpty() ? QStringLiteral("dialog-warning") : iconName));

    auto content = new QWidget(this);
    auto contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);

    if (!subtitle.isEmpty()) {
        auto subtitleLabel = new QLabel(subtitle, content);
        subtitleLabel->setObjectName(QStringLiteral("SubtitleText"));
        subtitleLabel->setAccessibleName(QStringLiteral("SubtitleText"));
        subtitleLabel->setTextFormat(Qt::PlainText);
        subtitleLabel->setWordWrap(true);
        QFont font = subtitleLabel->font();
        font.setBold(true);
        subtitleLabel->setFont(font);
        contentLayout->addWidget(subtitleLabel);
    }

    if (!body.isEmpty()) {
        auto bodyLabel = new QLabel(body, content);
        bodyLabel->setObjectName(QStringLiteral("BodyText"));
        bodyLabel->setAccessibleName(QStringLiteral("BodyText"));
        bodyLabel->setTextFormat(Qt::PlainText);
        bodyLabel->setWordWrap(true);
        contentLayout->addWidget(bodyLabel);
    }

    if (!m_choices.isEmpty()) {
        auto choicesWidget = new QWidget(content);
        auto choicesLayout = new QFormLayout(choicesWidget);
        choicesLayout->setContentsMargins(0, 0, 0, 0);
        for (const auto &option : std::as_const(m_choices)) {
            m_selectedChoices.insert(option.id, option.initialChoiceId);
            if (option.choices.isEmpty()) {
                auto checkBox = new QCheckBox(option.label, choicesWidget);
                checkBox->setChecked(option.initialChoiceId == QStringLiteral("true"));
                connect(checkBox, &QCheckBox::toggled, this, [this, id = option.id](bool checked) {
                    m_selectedChoices.insert(id,
                                             checked ? QStringLiteral("true")
                                                     : QStringLiteral("false"));
                });
                choicesLayout->addRow(checkBox);
                continue;
            }

            auto comboBox = new QComboBox(choicesWidget);
            int initialIndex = 0;
            for (qsizetype i = 0; i < option.choices.size(); ++i) {
                const auto &choice = option.choices.at(i);
                comboBox->addItem(choice.value, choice.id);
                if (choice.id == option.initialChoiceId) {
                    initialIndex = i;
                }
            }
            comboBox->setCurrentIndex(initialIndex);
            if (!option.choices.isEmpty()) {
                m_selectedChoices.insert(option.id, option.choices.at(initialIndex).id);
            }
            connect(comboBox, &QComboBox::currentIndexChanged, this,
                    [this, comboBox, id = option.id](int index) {
                if (index >= 0) {
                    m_selectedChoices.insert(id, comboBox->itemData(index).toString());
                }
            });
            choicesLayout->addRow(option.label, comboBox);
        }
        contentLayout->addWidget(choicesWidget);
    }

    addContent(content);

    const QString denyLabel = options.value(QStringLiteral("deny_label"), tr("Deny")).toString();
    const QString grantLabel = options.value(QStringLiteral("grant_label"), tr("Allow")).toString();
    const int denyButton = addButton(denyLabel, false, DDialog::ButtonNormal);
    const int grantButton = addButton(grantLabel, true, DDialog::ButtonRecommend);
    connect(this, &DDialog::buttonClicked, this,
            [this, denyButton, grantButton](int index, const QString &) {
        if (index == grantButton) {
            accept();
        } else if (index == denyButton) {
            reject();
        }
    });
}

AccessDialogTypes::Choices AccessDialog::selectedChoices() const
{
    AccessDialogTypes::Choices result;
    result.reserve(m_choices.size());
    for (const auto &option : m_choices) {
        result.append({option.id, m_selectedChoices.value(option.id, option.initialChoiceId)});
    }
    return result;
}
