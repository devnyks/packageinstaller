#include "alpmbackend.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryDir>

#pragma push_macro("signals")
#undef signals
#include <appstream.h>
#pragma pop_macro("signals")
#include <alpm.h>

#include <algorithm>
#include <cstdlib>

namespace {

QString textOrEmpty(const char* value) {
    return value ? QString::fromUtf8(value) : QString();
}

QStringList splitLines(const QString& value) {
    return value.split('\n', Qt::SkipEmptyParts);
}

QString appStreamIconName(AsComponent* component, QString* localPath, QString* remoteUrl) {
    auto useIcon = [localPath, remoteUrl](AsIcon* icon) -> QString {
        if (!icon) {
            return {};
        }
        const auto kind = as_icon_get_kind(icon);
        if (kind == AS_ICON_KIND_STOCK) {
            return textOrEmpty(as_icon_get_name(icon));
        }
        if (localPath && (kind == AS_ICON_KIND_LOCAL || kind == AS_ICON_KIND_CACHED)) {
            *localPath = textOrEmpty(as_icon_get_filename(icon));
        }
        if (remoteUrl && kind == AS_ICON_KIND_REMOTE) {
            *remoteUrl = textOrEmpty(as_icon_get_url(icon));
        }
        return {};
    };

    QString selectedName;
    if (auto* stock = as_component_get_icon_stock(component)) {
        selectedName = useIcon(stock);
    }
    if (auto* icons = as_component_get_icons(component)) {
        for (guint index = 0; index < icons->len; ++index) {
            if (const auto name = useIcon(AS_ICON(g_ptr_array_index(icons, index))); !name.isEmpty() && selectedName.isEmpty()) {
                selectedName = name;
            }
        }
    }
    return selectedName;
}

QString appStreamScreenshot(AsComponent* component) {
    auto* screenshots = as_component_get_screenshots_all(component);
    if (!screenshots) {
        return {};
    }
    for (guint index = 0; index < screenshots->len; ++index) {
        auto* screenshot = AS_SCREENSHOT(g_ptr_array_index(screenshots, index));
        auto* images = as_screenshot_get_images_all(screenshot);
        if (!images || images->len == 0) {
            continue;
        }
        auto* image = AS_IMAGE(g_ptr_array_index(images, 0));
        const auto url = textOrEmpty(as_image_get_url(image));
        if (!url.isEmpty()) {
            return url;
        }
    }
    return {};
}

void appendPrepareError(TransactionPreview& preview, alpm_errno_t error, alpm_list_t* data) {
    preview.error = QString::fromUtf8(alpm_strerror(error));
    if (error == ALPM_ERR_UNSATISFIED_DEPS && data) {
        for (auto* node = data; node; node = node->next) {
            auto* missing = static_cast<alpm_depmissing_t*>(node->data);
            if (!missing) {
                continue;
            }
            QString dependency;
            if (missing->depend) {
                char* dependencyText = alpm_dep_compute_string(missing->depend);
                dependency = textOrEmpty(dependencyText);
                std::free(dependencyText);
            }
            const auto target = textOrEmpty(missing->target);
            preview.conflicts << QObject::tr("%1 requires %2").arg(target, dependency);
        }
    } else if (error == ALPM_ERR_CONFLICTING_DEPS && data) {
        for (auto* node = data; node; node = node->next) {
            auto* conflict = static_cast<alpm_conflict_t*>(node->data);
            if (!conflict) {
                continue;
            }
            const auto first = conflict->package1 ? textOrEmpty(alpm_pkg_get_name(conflict->package1)) : QString();
            const auto second = conflict->package2 ? textOrEmpty(alpm_pkg_get_name(conflict->package2)) : QString();
            preview.conflicts << QObject::tr("%1 conflicts with %2").arg(first, second);
        }
    } else if (error == ALPM_ERR_FILE_CONFLICTS && data) {
        for (auto* node = data; node; node = node->next) {
            auto* conflict = static_cast<alpm_fileconflict_t*>(node->data);
            if (conflict) {
                preview.conflicts << QObject::tr("%1 conflicts with %2").arg(textOrEmpty(conflict->target), textOrEmpty(conflict->file));
            }
        }
    }
}

void freePrepareData(alpm_errno_t error, alpm_list_t* data) {
    if (!data) {
        return;
    }
    if (error == ALPM_ERR_UNSATISFIED_DEPS) {
        alpm_list_free_inner(data, reinterpret_cast<alpm_list_fn_free>(alpm_depmissing_free));
    } else if (error == ALPM_ERR_CONFLICTING_DEPS) {
        alpm_list_free_inner(data, reinterpret_cast<alpm_list_fn_free>(alpm_conflict_free));
    } else if (error == ALPM_ERR_FILE_CONFLICTS) {
        alpm_list_free_inner(data, reinterpret_cast<alpm_list_fn_free>(alpm_fileconflict_free));
    }
    alpm_list_free(data);
}

} // namespace

AlpmBackend::~AlpmBackend() {
    if (m_handle) {
        alpm_release(m_handle);
        m_handle = nullptr;
    }
    if (!m_snapshotDbPath.isEmpty()) {
        QDir(m_snapshotDbPath).removeRecursively();
    }
}

bool AlpmBackend::copyTree(const QString& source, const QString& destination, QString& error) {
    QDir sourceDir(source);
    if (!sourceDir.exists()) {
        error = QObject::tr("Pacman database directory does not exist: %1").arg(source);
        return false;
    }
    if (!QDir().mkpath(destination)) {
        error = QObject::tr("Cannot create temporary package database");
        return false;
    }

    const auto entries = sourceDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
    for (const auto& entry : entries) {
        const auto target = QDir(destination).filePath(entry.fileName());
        if (entry.isDir()) {
            if (!copyTree(entry.absoluteFilePath(), target, error)) {
                return false;
            }
        } else if (!QFile::copy(entry.absoluteFilePath(), target)) {
            error = QObject::tr("Cannot copy package database file: %1").arg(entry.fileName());
            return false;
        }
    }
    return true;
}

QString AlpmBackend::commandOutput(const QStringList& arguments, QString& error) {
    QProcess process;
    process.setProgram(QStringLiteral("pacman-conf"));
    process.setArguments(arguments);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start();
    if (!process.waitForStarted(2000) || !process.waitForFinished(5000)) {
        error = QObject::tr("Unable to query pacman configuration");
        return {};
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        error = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
        if (error.isEmpty()) {
            error = QObject::tr("pacman-conf returned an error");
        }
        return {};
    }
    return QString::fromLocal8Bit(process.readAllStandardOutput());
}

QString AlpmBackend::parseDirective(const QString& output, const QString& name) {
    for (const auto& line : splitLines(output)) {
        const auto trimmed = line.trimmed();
        if (!trimmed.startsWith(name + QStringLiteral(" ="))) {
            continue;
        }
        return trimmed.section('=', 1).trimmed();
    }
    return {};
}

QStringList AlpmBackend::parseDirectiveList(const QString& output, const QString& name) {
    QStringList result;
    for (const auto& line : splitLines(output)) {
        const auto trimmed = line.trimmed();
        if (trimmed.startsWith(name + QStringLiteral(" ="))) {
            const auto value = trimmed.section('=', 1).trimmed();
            if (!value.isEmpty()) {
                result << value;
            }
        }
    }
    return result;
}

QString AlpmBackend::formatSize(qint64 bytes) {
    if (bytes < 1024) {
        return QObject::tr("%1 B").arg(bytes);
    }
    if (bytes < 1024 * 1024) {
        return QObject::tr("%1 KiB").arg(bytes / 1024.0, 0, 'f', 1);
    }
    if (bytes < 1024 * 1024 * 1024) {
        return QObject::tr("%1 MiB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    }
    return QObject::tr("%1 GiB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1);
}

bool AlpmBackend::initialize(QString& error) {
    if (m_handle) {
        return true;
    }

    QString configError;
    const auto dbConfig = commandOutput({QStringLiteral("DBPath")}, configError);
    m_sourceDbPath = parseDirective(dbConfig, QStringLiteral("DBPath"));
    if (m_sourceDbPath.isEmpty()) {
        m_sourceDbPath = QStringLiteral("/var/lib/pacman");
    }
    m_sourceDbPath = QDir::cleanPath(m_sourceDbPath);

    const auto snapshotRoot = QDir(QDir::tempPath()).filePath(QStringLiteral("cachyos-catalog-pacman-XXXXXX"));
    QTemporaryDir temp(snapshotRoot);
    if (!temp.isValid()) {
        error = QObject::tr("Cannot create a temporary package database snapshot");
        return false;
    }
    const QString snapshotPath = temp.path();
    if (!copyTree(QDir(m_sourceDbPath).filePath(QStringLiteral("local")), QDir(snapshotPath).filePath(QStringLiteral("local")), error)
        || !copyTree(QDir(m_sourceDbPath).filePath(QStringLiteral("sync")), QDir(snapshotPath).filePath(QStringLiteral("sync")), error)) {
        return false;
    }
    temp.setAutoRemove(false);
    m_snapshotDbPath = snapshotPath;

    alpm_errno_t initError = ALPM_ERR_OK;
    const auto root = QByteArrayLiteral("/");
    const auto dbPath = m_snapshotDbPath.toLocal8Bit();
    m_handle = alpm_initialize(root.constData(), dbPath.constData(), &initError);
    if (!m_handle) {
        error = QObject::tr("Cannot initialize libalpm: %1").arg(QString::fromUtf8(alpm_strerror(initError)));
        return false;
    }

    QString optionsError;
    const auto optionsOutput = commandOutput({QStringLiteral("-v"), QStringLiteral("GPGDir"), QStringLiteral("Architecture")}, optionsError);
    const auto gpgDir = parseDirective(optionsOutput, QStringLiteral("GPGDir"));
    if (!gpgDir.isEmpty()) {
        alpm_option_set_gpgdir(m_handle, gpgDir.toLocal8Bit().constData());
    }
    const auto architectures = parseDirectiveList(optionsOutput, QStringLiteral("Architecture"));
    if (!architectures.isEmpty()) {
        alpm_list_t* architectureList = nullptr;
        for (const auto& architecture : architectures) {
            alpm_list_append_strdup(&architectureList, architecture.toLocal8Bit().constData());
        }
        alpm_option_set_architectures(m_handle, architectureList);
        alpm_list_free_inner(architectureList, free);
        alpm_list_free(architectureList);
    }
    alpm_option_set_default_siglevel(m_handle, ALPM_SIG_PACKAGE | ALPM_SIG_DATABASE | ALPM_SIG_DATABASE_OPTIONAL);
    QString reposError;
    const auto reposOutput = commandOutput({QStringLiteral("--repo-list")}, reposError);
    const auto repos = splitLines(reposOutput);
    if (repos.isEmpty()) {
        error = reposError.isEmpty() ? QObject::tr("No pacman repositories are configured") : reposError;
        return false;
    }

    for (const auto& repo : repos) {
        const auto cleanRepo = repo.trimmed();
        if (cleanRepo.isEmpty()) {
            continue;
        }
        const auto serverOutput = commandOutput({QStringLiteral("-r"), cleanRepo, QStringLiteral("-v"), QStringLiteral("Server")}, reposError);
        auto* db = alpm_register_syncdb(m_handle, cleanRepo.toLocal8Bit().constData(), ALPM_SIG_USE_DEFAULT);
        if (!db) {
            continue;
        }
        for (const auto& server : parseDirectiveList(serverOutput, QStringLiteral("Server"))) {
            alpm_db_add_server(db, server.toLocal8Bit().constData());
        }
        alpm_db_set_usage(db, ALPM_DB_USAGE_ALL);
    }

    if (!alpm_get_syncdbs(m_handle)) {
        error = QObject::tr("No pacman sync databases are available");
        return false;
    }
    return true;
}

bool AlpmBackend::reload(QString& error) {
    if (m_handle) {
        alpm_release(m_handle);
        m_handle = nullptr;
    }
    if (!m_snapshotDbPath.isEmpty()) {
        QDir(m_snapshotDbPath).removeRecursively();
        m_snapshotDbPath.clear();
    }
    return initialize(error);
}

QHash<QString, AppMetadata> AlpmBackend::loadAppStream() const {
    QHash<QString, AppMetadata> result;
    auto* pool = as_pool_new();
    as_pool_set_locale(pool, qgetenv("LANG").isEmpty() ? "en_US" : qgetenv("LANG").constData());
    as_pool_set_flags(pool, static_cast<AsPoolFlags>(AS_POOL_FLAG_LOAD_OS_CATALOG | AS_POOL_FLAG_LOAD_OS_METAINFO | AS_POOL_FLAG_LOAD_OS_DESKTOP_FILES | AS_POOL_FLAG_LOAD_FLATPAK));

    GError* appstreamError = nullptr;
    if (!as_pool_load(pool, nullptr, &appstreamError)) {
        if (appstreamError) {
            g_error_free(appstreamError);
        }
        g_object_unref(pool);
        return result;
    }

    auto* components = as_pool_get_components(pool);
    for (guint index = 0; components && index < as_component_box_get_size(components); ++index) {
        auto* component = as_component_box_index_safe(components, index);
        if (!component || as_component_get_kind(component) != AS_COMPONENT_KIND_DESKTOP_APP) {
            continue;
        }

        AppMetadata metadata;
        metadata.id = textOrEmpty(as_component_get_id(component));
        metadata.name = textOrEmpty(as_component_get_name(component));
        metadata.summary = textOrEmpty(as_component_get_summary(component));
        metadata.description = textOrEmpty(as_component_get_description(component));
        metadata.homepage = textOrEmpty(as_component_get_url(component, AS_URL_KIND_HOMEPAGE));
        metadata.license = textOrEmpty(as_component_get_project_license(component));
        metadata.iconName = appStreamIconName(component, &metadata.iconPath, &metadata.iconUrl);
        if (!metadata.iconPath.isEmpty() && !QFileInfo::exists(metadata.iconPath)) {
            metadata.iconPath.clear();
        }
        metadata.screenshotUrl = appStreamScreenshot(component);
        if (!metadata.id.isEmpty()) {
            result.insert(metadata.id, metadata);
            const auto idAlias = metadata.id.section('.', -1).toLower();
            if (!idAlias.isEmpty() && !result.contains(idAlias)) {
                result.insert(idAlias, metadata);
            }
        }

        const auto addPackage = [&result, &metadata](const char* package) {
            if (package && *package && !result.contains(QString::fromUtf8(package))) {
                result.insert(QString::fromUtf8(package), metadata);
            }
        };
        addPackage(as_component_get_pkgname(component));
        if (auto* packageNames = as_component_get_pkgnames(component)) {
            for (gchar** package = packageNames; *package; ++package) {
                addPackage(*package);
            }
        }
    }
    if (components) {
        g_object_unref(components);
    }
    g_object_unref(pool);
    return result;
}

PackageSnapshot AlpmBackend::loadSnapshot() {
    PackageSnapshot snapshot;
    if (!m_handle) {
        snapshot.error = QObject::tr("libalpm is not initialized");
        return snapshot;
    }

    if (!m_metadataLoaded) {
        m_metadata = loadAppStream();
        m_metadataLoaded = true;
    }
    snapshot.metadata = m_metadata;
    auto* localDb = alpm_get_localdb(m_handle);
    auto* syncDbs = alpm_get_syncdbs(m_handle);
    QSet<QString> seen;

    for (auto* dbNode = syncDbs; dbNode; dbNode = dbNode->next) {
        auto* db = static_cast<alpm_db_t*>(dbNode->data);
        const auto repoName = textOrEmpty(alpm_db_get_name(db));
        auto* packageCache = alpm_db_get_pkgcache(db);
        for (auto* packageNode = packageCache; packageNode; packageNode = packageNode->next) {
            auto* package = static_cast<alpm_pkg_t*>(packageNode->data);
            const auto name = textOrEmpty(alpm_pkg_get_name(package));
            if (name.isEmpty() || seen.contains(name)) {
                continue;
            }
            seen.insert(name);

            CatalogItem item;
            item.id = QStringLiteral("native:") + name;
            item.name = name;
            item.version = textOrEmpty(alpm_pkg_get_version(package));
            item.description = textOrEmpty(alpm_pkg_get_desc(package));
            item.summary = item.description;
            item.source = QStringLiteral("native");
            item.repo = repoName;
            item.targets = {name};
            item.downloadSize = alpm_pkg_get_size(package);
            item.installedSize = alpm_pkg_get_isize(package);
            item.homepage = textOrEmpty(alpm_pkg_get_url(package));
            if (auto* licenses = alpm_pkg_get_licenses(package)) {
                for (auto* licenseNode = licenses; licenseNode; licenseNode = licenseNode->next) {
                    if (!item.license.isEmpty()) {
                        item.license += QStringLiteral(", ");
                    }
                    item.license += textOrEmpty(static_cast<const char*>(licenseNode->data));
                }
            }
            if (auto* local = alpm_db_get_pkg(localDb, name.toLocal8Bit().constData())) {
                item.installed = true;
                item.installedVersion = textOrEmpty(alpm_pkg_get_version(local));
                item.upgradable = alpm_sync_get_new_version(local, syncDbs) != nullptr;
            }
            item.status = item.upgradable ? QStringLiteral("upgradable") : item.installed ? QStringLiteral("installed") : QStringLiteral("available");

            if (const auto app = m_metadata.value(name); !app.id.isEmpty()) {
                item.name = app.name.isEmpty() ? item.name : app.name;
                item.summary = app.summary.isEmpty() ? item.summary : app.summary;
                item.description = app.description.isEmpty() ? item.description : app.description;
                item.iconName = app.iconName;
                item.iconPath = app.iconPath;
                item.iconUrl = app.iconUrl;
                item.screenshotUrl = app.screenshotUrl;
                item.homepage = app.homepage;
                item.license = app.license;
            }
            item.sizeText = formatSize(item.downloadSize);
            snapshot.packages.push_back(std::move(item));
        }
    }

    if (auto* localPackages = alpm_db_get_pkgcache(localDb)) {
        snapshot.installed.reserve(1000);
        for (auto* node = localPackages; node; node = node->next) {
            if (auto* package = static_cast<alpm_pkg_t*>(node->data)) {
                snapshot.installed << textOrEmpty(alpm_pkg_get_name(package));
            }
        }
    }
    std::sort(snapshot.packages.begin(), snapshot.packages.end(), [](const auto& first, const auto& second) {
        return first.name.toCaseFolded() < second.name.toCaseFolded();
    });
    snapshot.valid = !snapshot.packages.isEmpty();
    if (!snapshot.valid) {
        snapshot.error = QObject::tr("Pacman sync databases contain no packages");
    }
    return snapshot;
}

TransactionPreview AlpmBackend::previewInstall(const QStringList& targets) {
    return preview(targets, QStringLiteral("install"), false, false);
}

TransactionPreview AlpmBackend::previewRemove(const QStringList& targets) {
    return preview(targets, QStringLiteral("remove"), true, false);
}

TransactionPreview AlpmBackend::previewSystemUpgrade() {
    return preview({}, QStringLiteral("upgrade"), false, true);
}

TransactionPreview AlpmBackend::preview(const QStringList& targets, const QString& action, bool remove, bool systemUpgrade) {
    TransactionPreview result;
    result.action = action;
    if (!m_handle) {
        result.error = QObject::tr("libalpm is not initialized");
        return result;
    }
    if (QFile::exists(QDir(m_sourceDbPath).filePath(QStringLiteral("db.lck")))) {
        result.error = QObject::tr("Another package operation is using the pacman database");
        return result;
    }

    const int flags = ALPM_TRANS_FLAG_DBONLY | ALPM_TRANS_FLAG_NOLOCK
        | ALPM_TRANS_FLAG_ALLDEPS | ALPM_TRANS_FLAG_ALLEXPLICIT;
    if (alpm_trans_init(m_handle, flags) != 0) {
        result.error = QString::fromUtf8(alpm_strerror(alpm_errno(m_handle)));
        return result;
    }

    bool targetError = false;
    if (systemUpgrade) {
        targetError = alpm_sync_sysupgrade(m_handle, 0) != 0;
    } else if (remove) {
        auto* local = alpm_get_localdb(m_handle);
        for (const auto& target : targets) {
            const auto bytes = target.toLocal8Bit();
            auto* package = alpm_db_get_pkg(local, bytes.constData());
            if (!package || alpm_remove_pkg(m_handle, package) != 0) {
                result.conflicts << QObject::tr("Package is not installed: %1").arg(target);
                targetError = true;
            }
        }
    } else {
        auto* dbs = alpm_get_syncdbs(m_handle);
        for (const auto& target : targets) {
            const auto bytes = target.toLocal8Bit();
            if (auto* package = alpm_find_dbs_satisfier(m_handle, dbs, bytes.constData())) {
                if (alpm_add_pkg(m_handle, package) != 0) {
                    targetError = true;
                }
                continue;
            }
            bool groupFound = false;
            for (auto* dbNode = dbs; dbNode; dbNode = dbNode->next) {
                auto* db = static_cast<alpm_db_t*>(dbNode->data);
                if (auto* group = alpm_db_get_group(db, bytes.constData())) {
                    groupFound = true;
                    for (auto* packageNode = group->packages; packageNode; packageNode = packageNode->next) {
                        if (alpm_add_pkg(m_handle, static_cast<alpm_pkg_t*>(packageNode->data)) != 0) {
                            targetError = true;
                        }
                    }
                    break;
                }
            }
            if (!groupFound) {
                result.conflicts << QObject::tr("Target not found: %1").arg(target);
                targetError = true;
            }
        }
    }

    if (targetError) {
        result.error = QObject::tr("One or more targets could not be added to the transaction");
        alpm_trans_release(m_handle);
        return result;
    }

    alpm_list_t* data = nullptr;
    const int prepared = alpm_trans_prepare(m_handle, &data);
    const auto prepareError = alpm_errno(m_handle);
    if (prepared != 0) {
        appendPrepareError(result, prepareError, data);
        freePrepareData(prepareError, data);
        alpm_trans_release(m_handle);
        return result;
    }

    for (auto* node = alpm_trans_get_add(m_handle); node; node = node->next) {
        auto* package = static_cast<alpm_pkg_t*>(node->data);
        if (!package) {
            continue;
        }
        const auto name = textOrEmpty(alpm_pkg_get_name(package));
        const auto version = textOrEmpty(alpm_pkg_get_version(package));
        result.additions << (name + QStringLiteral(" ") + version);
        result.downloadSize += alpm_pkg_download_size(package);
        result.installedSize += alpm_pkg_get_isize(package);
    }
    for (auto* node = alpm_trans_get_remove(m_handle); node; node = node->next) {
        auto* package = static_cast<alpm_pkg_t*>(node->data);
        if (!package) {
            continue;
        }
        const auto name = textOrEmpty(alpm_pkg_get_name(package));
        const auto version = textOrEmpty(alpm_pkg_get_version(package));
        result.removals << (name + QStringLiteral(" ") + version);
        result.removedSize += alpm_pkg_get_isize(package);
    }
    freePrepareData(prepareError, data);
    alpm_trans_release(m_handle);

    result.summary = QObject::tr("Download: %1\nInstalled: %2\nRemoved: %3")
        .arg(formatSize(result.downloadSize), formatSize(result.installedSize), formatSize(result.removedSize));
    result.success = true;
    return result;
}
