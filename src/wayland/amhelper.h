// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "toplevelmodel.h"

#include <QString>

namespace AMHelpers
{
QString getLocaleOrDefaultValue(const QStringMap &value, const QString &targetKey, const QString &fallbackKey);
void updateInfoFromAM(const QString &appID, QString &name, QString &icon);
}
