// Copyright © 2018 Red Hat, Inc
// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef XDG_DESKTOP_PORTAL_KDE_SCREENSHOT_DIALOG_H
#define XDG_DESKTOP_PORTAL_KDE_SCREENSHOT_DIALOG_H

#include <QDialog>

namespace Ui
{
class ScreenshotDialog;
}

class ScreenshotDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ScreenshotDialog(QDialog *parent = nullptr, Qt::WindowFlags flags = {});
    ~ScreenshotDialog();

    QImage image() const;

public Q_SLOTS:
    void takeScreenshot();

Q_SIGNALS:
    void failed();

private:
    Ui::ScreenshotDialog * m_dialog;
    QImage m_image;

    int mask();
};

#endif // XDG_DESKTOP_PORTAL_KDE_SCREENSHOT_DIALOG_H


