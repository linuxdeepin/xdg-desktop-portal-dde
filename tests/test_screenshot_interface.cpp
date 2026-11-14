// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "wayland/screenshotportal.h"
#include "wayland/protocols/common.h"

#include <QMetaMethod>
#include <QMetaProperty>

#include <iostream>

namespace
{

bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

}

int main()
{
    if (!expect(isSafeShmBufferLayout(1, 1, 4),
                "A valid shared-memory layout was rejected")
        || !expect(!isSafeShmBufferLayout(0, 1, 0),
                   "A zero-width shared-memory layout was accepted")
        || !expect(!isSafeShmBufferLayout(2, 1, 4),
                   "A mismatched shared-memory stride was accepted")
        || !expect(!isSafeShmBufferLayout(50000, 50000, 200000),
                   "An overflowing Qt shared-memory allocation was accepted")) {
        return 1;
    }

    const QMetaObject &metaObject = ScreenshotPortalWayland::staticMetaObject;
    const int versionIndex = metaObject.indexOfProperty("version");
    if (!expect(versionIndex >= 0, "Screenshot version property is missing")) {
        return 1;
    }

    const QMetaProperty version = metaObject.property(versionIndex);
    if (!expect(version.metaType() == QMetaType::fromType<uint>(),
                "Screenshot version property has the wrong type")
        || !expect(version.isReadable() && !version.isWritable(),
                   "Screenshot version property must be read-only")) {
        return 1;
    }

    bool foundDelayedScreenshot = false;
    for (int i = metaObject.methodOffset(); i < metaObject.methodCount(); ++i) {
        const QMetaMethod method = metaObject.method(i);
        if (method.name() != QByteArrayLiteral("Screenshot")) {
            continue;
        }
        const QList<QByteArray> parameters = method.parameterTypes();
        foundDelayedScreenshot = method.returnType() == QMetaType::Void
                && parameters.contains(QByteArrayLiteral("QDBusMessage"))
                && parameters.contains(QByteArrayLiteral("uint&"))
                && parameters.contains(QByteArrayLiteral("QVariantMap&"));
    }

    return expect(foundDelayedScreenshot,
                  "Screenshot is not exposed with the delayed-reply signature")
            ? 0
            : 1;
}
