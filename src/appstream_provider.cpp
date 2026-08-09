#include "appstream_provider.h"

#include "logging.h"

// glib headers declare a `signals` member that collides with the Qt
// Q_SIGNALS macro (`signals` -> `public`); undef before pulling glib in.
#ifdef signals
#undef signals
#endif

#include <appstream.h>

#include <QHash>
#include <QtConcurrent>

#include <optional>

namespace {

/// Convert a GLib component into our lightweight POD snapshot. The snapshot
/// is taken on the loading thread and all GLib objects are unreferenced
/// afterwards, so the UI can read the data from the main thread safely.
AppStreamProvider::Component snapshotComponent(AsComponent* c) {
    AppStreamProvider::Component out;
    out.id = QString::fromUtf8(as_component_get_id(c));
    const gchar* name = as_component_get_name(c);
    out.name = name ? QString::fromUtf8(name) : out.id.section(QLatin1Char('.'), -1);
    const gchar* summary = as_component_get_summary(c);
    out.summary = summary ? QString::fromUtf8(summary) : QString();
    const gchar* desc = as_component_get_description(c);
    if (desc) {
        // plain-text fallback; AppStream may return a descriptive string
        out.description = QString::fromUtf8(desc).trimmed();
    }
    out.developer = QString();
    AsDeveloper* dev = as_component_get_developer(c);
    if (dev) {
        const gchar* name = as_developer_get_name(dev);
        out.developer = name ? QString::fromUtf8(name) : QString();
    }

    const gchar* url = as_component_get_url(c, AS_URL_KIND_HOMEPAGE);
    out.homepage = url ? QString::fromUtf8(url) : QString();
    url = as_component_get_url(c, AS_URL_KIND_BUGTRACKER);
    out.projectUrl = url ? QString::fromUtf8(url) : QString();

    // icon: prefer a concrete file (flatpak), fall back to a theme name
    GPtrArray* icons = as_component_get_icons(c);
    if (icons) {
        for (guint i = 0; i < icons->len; ++i) {
            AsIcon* ic = AS_ICON(icons->pdata[i]);
            if (as_icon_get_kind(ic) == AS_ICON_KIND_LOCAL) {
                const gchar* file = as_icon_get_filename(ic);
                if (file) {
                    out.iconFile = QString::fromUtf8(file);
                    break;
                }
            } else if (as_icon_get_kind(ic) == AS_ICON_KIND_STOCK || as_icon_get_kind(ic) == AS_ICON_KIND_CACHED) {
                const gchar* nameIcon = as_icon_get_name(ic);
                if (nameIcon && out.iconName.isEmpty()) {
                    out.iconName = QString::fromUtf8(nameIcon);
                }
            }
        }
    }

    GPtrArray* shots = as_component_get_screenshots_all(c);
    if (shots) {
        for (guint i = 0; i < shots->len; ++i) {
            AsScreenshot* shot = AS_SCREENSHOT(shots->pdata[i]);
            AsImage* img = as_screenshot_get_image(shot, 0, 0, 1);
            if (!img) {
                continue;
            }
            const gchar* url = as_image_get_url(img);
            if (url) {
                out.screenshots << QString::fromUtf8(url);
            }
        }
    }
    return out;
}

}  // namespace

struct AppStreamProvider::Impl {
    QHash<QString, int> byIdIndex;
    QHash<QString, QVector<int>> byPkgIndex;
    QVector<Component> components;
};

AppStreamProvider::AppStreamProvider(QObject* parent)
    : QObject(parent), m_impl(std::make_unique<Impl>()) {}

AppStreamProvider::~AppStreamProvider() = default;

void AppStreamProvider::loadAsync() {
    // Heavy synchronous load in a worker thread; the UI stays responsive.
    QtConcurrent::run([this] {
        AsPool* pool = as_pool_new();
        as_pool_set_flags(pool, static_cast<AsPoolFlags>(AS_POOL_FLAG_LOAD_OS_CATALOG | AS_POOL_FLAG_LOAD_OS_METAINFO
                | AS_POOL_FLAG_LOAD_FLATPAK));
        const bool ok = as_pool_load(pool, nullptr, nullptr);
        logging::debug(QStringLiteral("appstream pool load: %1")
                           .arg(ok ? QStringLiteral("ok") : QStringLiteral("failed")));

        // Snapshot everything into plain Qt data on this thread.
        QVector<Component> snapshot;
        QHash<QString, int> byId;
        QHash<QString, QVector<int>> byPkg;
        if (ok) {
            AsComponentBox* all = as_pool_get_components(pool);
            const guint n = as_component_box_get_size(all);
            snapshot.reserve(n);
            for (guint i = 0; i < n; ++i) {
                AsComponent* c = as_component_box_index_safe(all, i);
                const QString id = QString::fromUtf8(as_component_get_id(c));
                const int idx = snapshot.size();
                byId.insert(id, idx);
                gchar** pkgs = as_component_get_pkgnames(c);
                if (pkgs) {
                    for (gchar** p = pkgs; *p; ++p) {
                        byPkg[QString::fromUtf8(*p)].append(idx);
                    }
                    g_strfreev(pkgs);
                }
                snapshot.append(snapshotComponent(c));
            }
            logging::debug(QStringLiteral("appstream components: %1").arg(snapshot.size()));
        }
        g_object_unref(pool);

        // Hand the snapshot to the main thread.
        QMetaObject::invokeMethod(this, [this, snapshot = std::move(snapshot), byId = std::move(byId), byPkg = std::move(byPkg)] {
            m_impl->components = snapshot;
            m_impl->byIdIndex = byId;
            m_impl->byPkgIndex = byPkg;
            m_ready = true;
            emit ready();
        }, Qt::QueuedConnection);
    });
}

std::optional<AppStreamProvider::Component> AppStreamProvider::byId(const QString& id) const {
    const auto it = m_impl->byIdIndex.constFind(id);
    if (it != m_impl->byIdIndex.constEnd()) {
        return m_impl->components.at(it.value());
    }
    return std::nullopt;
}

std::optional<AppStreamProvider::Component> AppStreamProvider::byPackage(const QString& pkgname) const {
    const auto it = m_impl->byPkgIndex.constFind(pkgname);
    if (it != m_impl->byPkgIndex.constEnd() && !it.value().isEmpty()) {
        return m_impl->components.at(it.value().first());
    }
    return std::nullopt;
}
