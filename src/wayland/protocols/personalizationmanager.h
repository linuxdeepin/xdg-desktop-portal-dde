// Copyright (C) 2024 Wenhao Peng <pengwenhao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qwayland-treeland-personalization-manager-v1.h"

#include <QScreen>
#include <QtWaylandClient/QWaylandClientExtension>

class PersonalizationWallpaperContext;
class PersonalizationManager : public QWaylandClientExtensionTemplate<PersonalizationManager>,
                               public QtWayland::treeland_personalization_manager_v1
{
    Q_OBJECT
public:
    explicit PersonalizationManager();
    ~PersonalizationManager();
    void onActiveChanged();

    PersonalizationWallpaperContext *wallpaper() { return m_wallpaperContext; }

private:
    PersonalizationWallpaperContext *m_wallpaperContext = nullptr;
};

class PersonalizationWindowContext : public QWaylandClientExtensionTemplate<PersonalizationWindowContext>,
                                     public QtWayland::treeland_personalization_window_context_v1
{
    Q_OBJECT
public:
    explicit PersonalizationWindowContext(struct ::treeland_personalization_window_context_v1 *context);
};

class PersonalizationWallpaperContext : public QWaylandClientExtensionTemplate<PersonalizationWallpaperContext>,
                                        public QtWayland::treeland_personalization_wallpaper_context_v1
{
    Q_OBJECT
public:
    explicit PersonalizationWallpaperContext(struct ::treeland_personalization_wallpaper_context_v1 *context);

Q_SIGNALS:
    void metadataChanged(const QString &meta);

protected:
    void treeland_personalization_wallpaper_context_v1_metadata(const QString &metadata) override;
};
