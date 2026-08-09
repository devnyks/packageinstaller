#include "alpm_manager.h"

#include "logging.h"
#include "pacman_conf.h"

#include <alpm.h>

#include <QCoreApplication>
#include <QFile>
#include <QMap>
#include <QMutex>
#include <QSet>

#include <cstdarg>
#include <sys/utsname.h>
#include <cstring>

namespace {

constexpr int kTransFlags =
    ALPM_TRANS_FLAG_DBONLY | ALPM_TRANS_FLAG_ALLDEPS | ALPM_TRANS_FLAG_ALLEXPLICIT | ALPM_TRANS_FLAG_NOLOCK;

QString formatVa(const char* fmt, va_list args) {
    va_list copy;
    va_copy(copy, args);
    const int size = vsnprintf(nullptr, 0, fmt, copy);
    va_end(copy);
    if (size <= 0) {
        return {};
    }
    std::string buf(size + 1, '\0');
    vsnprintf(buf.data(), size + 1, fmt, args);
    return QString::fromUtf8(buf.c_str());
}

void cbLog(void*, alpm_loglevel_t level, const char* fmt, va_list args) {
    if (level == ALPM_LOG_DEBUG || level == ALPM_LOG_FUNCTION) {
        return;
    }
    const QString msg = QStringLiteral("ALPM: %1").arg(formatVa(fmt, args));
    if (level == ALPM_LOG_ERROR) {
        logging::error(msg);
    } else if (level == ALPM_LOG_WARNING) {
        logging::warn(msg);
    }
}

void cbEvent(void*, alpm_event_t* event) {
    switch (event->type) {
    case ALPM_EVENT_CHECKDEPS_START: logging::info(QStringLiteral("ALPM: checking dependencies...")); break;
    case ALPM_EVENT_RESOLVEDEPS_START: logging::info(QStringLiteral("ALPM: resolving dependencies...")); break;
    case ALPM_EVENT_INTERCONFLICTS_START: logging::info(QStringLiteral("ALPM: looking for conflicting packages...")); break;
    case ALPM_EVENT_TRANSACTION_START: logging::info(QStringLiteral("ALPM: :: Processing package changes...")); break;
    case ALPM_EVENT_FILECONFLICTS_START: logging::info(QStringLiteral("ALPM: ALPM_EVENT_FILECONFLICTS_START")); break;
    case ALPM_EVENT_FILECONFLICTS_DONE: logging::info(QStringLiteral("ALPM: ALPM_EVENT_FILECONFLICTS_DONE")); break;
    case ALPM_EVENT_INTERCONFLICTS_DONE: logging::info(QStringLiteral("ALPM: ALPM_EVENT_INTERCONFLICTS_DONE")); break;
    default: break;
    }
}

QString displaySizes(off_t number) {
    if (number < 1024) {
        return QString::number(number);
    }
    if (number < 1024 * 1024) {
        return QStringLiteral("%1 KB").arg(number / 1024);
    }
    if (number < 1024 * 1024 * 1024) {
        return QStringLiteral("%1 MB").arg(number / 1024 / 1024);
    }
    return QStringLiteral("%1 GB").arg(number / 1024 / 1024 / 1024);
}

}  // namespace

namespace alpm {

AlpmManager::~AlpmManager() {
    if (m_handle) {
        alpm_release(static_cast<alpm_handle_t*>(m_handle));
    }
}

AlpmManager::AlpmManager(AlpmManager&& other) noexcept : m_handle(other.m_handle) {
    other.m_handle = nullptr;
}

AlpmManager& AlpmManager::operator=(AlpmManager&& other) noexcept {
    if (this != &other) {
        if (m_handle) {
            alpm_release(static_cast<alpm_handle_t*>(m_handle));
        }
        m_handle = other.m_handle;
        other.m_handle = nullptr;
    }
    return *this;
}

std::optional<AlpmManager> AlpmManager::init() {
    alpm_errno_t err = static_cast<alpm_errno_t>(0);
    alpm_handle_t* handle = alpm_initialize("/", "/var/lib/pacman/", &err);
    if (!handle) {
        logging::error(QStringLiteral("failed to init ALPM handle: %1").arg(alpm_strerror(static_cast<alpm_errno_t>(err))));
        return std::nullopt;
    }
    AlpmManager manager(handle);
    manager.registerSyncDbs();
    manager.setupCallbacks();
    return manager;
}

void AlpmManager::registerSyncDbs() {
    auto* handle = static_cast<alpm_handle_t*>(m_handle);
    const PacmanConfig cfg = PacmanConfig::parse(QStringLiteral("/etc/pacman.conf"));
    for (const auto& repo : cfg.repos) {
        alpm_db_t* db = alpm_register_syncdb(handle, repo.name.toUtf8().constData(), ALPM_DB_USAGE_ALL);
        if (!db) {
            logging::warn(QStringLiteral("failed to register sync db %1: %2")
                              .arg(repo.name, alpm_strerror(alpm_errno(handle))));
            continue;
        }
        alpm_list_t* servers = nullptr;
        for (const auto& s : repo.servers) {
            servers = alpm_list_add(servers, strdup(s.toUtf8().constData()));
        }
        if (servers) {
            alpm_db_set_servers(db, servers);
            alpm_list_free_inner(servers, free);
            alpm_list_free(servers);
        }
    }
    // `Architecture = auto` resolves to the machine arch plus the CPU
    // micro-architecture level (CachyOS ships x86_64_v3 packages; pacman on
    // CachyOS exposes the detected level to libalpm).
    if (!cfg.architectures.isEmpty()) {
        struct utsname un;
        const QString machine = (uname(&un) == 0) ? QString::fromUtf8(un.machine) : QStringLiteral("x86_64");
        QStringList arches;
        for (const auto& a : cfg.architectures) {
            if (a != QLatin1String("auto")) {
                arches << a;
                continue;
            }
            arches << machine;
            if (machine == QLatin1String("x86_64")) {
                // detect micro-architecture level from /proc/cpuinfo flags
                QStringList flags;
                QFile cpuinfo(QStringLiteral("/proc/cpuinfo"));
                if (cpuinfo.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    const auto lines = QString::fromUtf8(cpuinfo.readAll()).split(QLatin1Char('\n'));
                    for (const auto& l : lines) {
                        if (l.startsWith(QLatin1String("flags"))) {
                            flags = l.mid(l.indexOf(QLatin1Char(':')) + 1).simplified().split(QLatin1Char(' '));
                            break;
                        }
                    }
                }
                const auto has = [&flags](const char* f) { return flags.contains(QLatin1String(f)); };
                if (has("avx512f")) {
                    arches << QStringLiteral("x86_64_v4") << QStringLiteral("x86_64_v3") << QStringLiteral("x86_64_v2");
                } else if (has("avx2")) {
                    arches << QStringLiteral("x86_64_v3") << QStringLiteral("x86_64_v2");
                } else if (has("sse4_2")) {
                    arches << QStringLiteral("x86_64_v2");
                }
            }
        }
        for (const auto& arch : arches) {
            // alpm_option_add_architecture copies the string (the set_*
            // variant takes ownership of the list, which we must avoid)
            alpm_option_add_architecture(handle, arch.toUtf8().constData());
        }
    }
}

void AlpmManager::setupCallbacks() {
    auto* handle = static_cast<alpm_handle_t*>(m_handle);
    alpm_option_set_logcb(handle, cbLog, nullptr);
    alpm_option_set_eventcb(handle, cbEvent, nullptr);
}

void AlpmManager::refresh() {
    QMutexLocker lock(&m_mutex);
    auto* old = static_cast<alpm_handle_t*>(m_handle);
    alpm_errno_t err = static_cast<alpm_errno_t>(0);
    alpm_handle_t* handle = alpm_initialize("/", "/var/lib/pacman/", &err);
    if (!handle) {
        logging::error(QStringLiteral("failed to refresh ALPM handle: %1").arg(alpm_strerror(static_cast<alpm_errno_t>(err))));
        return;
    }
    m_handle = handle;
    alpm_release(old);
    registerSyncDbs();
    setupCallbacks();
}

namespace {

/// Adds every resolvable target (package, repo-qualified package or group)
/// to the current transaction, mirroring reference `add_targets_to_install`
/// but resolving *all* targets (the reference only added the first match;
/// multi-package previews need every selected package, see DESIGN.md).
bool addTargetsToInstall(alpm_handle_t* handle, const QStringList& targets) {
    const auto addPkg = [handle](alpm_pkg_t* pkg) -> bool {
        if (alpm_add_pkg(handle, pkg) != 0) {
            logging::error(QStringLiteral("error: '%1': %2")
                               .arg(QString::fromUtf8(alpm_pkg_get_name(pkg)),
                                    alpm_strerror(alpm_errno(handle))));
            return false;
        }
        return true;
    };

    for (const QString& targ : targets) {
        const QString name = targ.section(QLatin1Char('/'), -1);
        alpm_list_t* dbs = alpm_get_syncdbs(handle);
        alpm_db_t* repoDb = nullptr;
        if (targ.contains(QLatin1Char('/'))) {
            const QString repo = targ.section(QLatin1Char('/'), 0, 0);
            for (alpm_list_t* d = dbs; d; d = d->next) {
                auto* db = static_cast<alpm_db_t*>(d->data);
                if (repo == QString::fromUtf8(alpm_db_get_name(db))) {
                    repoDb = db;
                    break;
                }
            }
            if (!repoDb) {
                logging::error(QStringLiteral("database not found: %1").arg(repo));
                continue;
            }
        }

        alpm_pkg_t* pkg = nullptr;
        if (repoDb) {
            pkg = alpm_db_get_pkg(repoDb, name.toUtf8().constData());
        } else {
            for (alpm_list_t* d = dbs; d && !pkg; d = d->next) {
                pkg = alpm_db_get_pkg(static_cast<alpm_db_t*>(d->data), name.toUtf8().constData());
            }
        }
        if (pkg) {
            if (!addPkg(pkg)) {
                return false;
            }
            continue;
        }

        // fall back to group targets (pacman -S semantics)
        if (repoDb) {
            alpm_group_t* group = alpm_db_get_group(repoDb, name.toUtf8().constData());
            if (group) {
                for (alpm_list_t* pk = group->packages; pk; pk = pk->next) {
                    if (!addPkg(static_cast<alpm_pkg_t*>(pk->data))) {
                        return false;
                    }
                }
                continue;
            }
        } else {
            // search the group in all sync dbs
            alpm_list_t* groupPkgs = alpm_find_group_pkgs(alpm_get_syncdbs(handle), name.toUtf8().constData());
            if (groupPkgs) {
                for (alpm_list_t* p = groupPkgs; p; p = p->next) {
                    if (!addPkg(static_cast<alpm_pkg_t*>(p->data))) {
                        alpm_list_free(groupPkgs);
                        return false;
                    }
                }
                alpm_list_free(groupPkgs);
                continue;
            }
        }
        logging::warn(QStringLiteral("target not found: %1").arg(targ));
    }
    return true;
}

bool addTargetsToRemove(alpm_handle_t* handle, const QStringList& targets) {
    alpm_db_t* local = alpm_get_localdb(handle);
    for (const QString& targ : targets) {
        alpm_pkg_t* pkg = alpm_db_get_pkg(local, targ.toUtf8().constData());
        if (!pkg) {
            logging::warn(QStringLiteral("package not installed: %1").arg(targ));
            continue;
        }
        if (alpm_remove_pkg(handle, pkg) != 0) {
            logging::error(QStringLiteral("failed to add package to be removed (%1): %2")
                               .arg(targ, alpm_strerror(alpm_errno(handle))));
            return false;
        }
    }
    return true;
}

}  // namespace

AlpmManager::Preview AlpmManager::displayInstallTargets(const QStringList& targets, bool verbose) {
    QMutexLocker lock(&m_mutex);
    Preview out;
    auto* handle = static_cast<alpm_handle_t*>(m_handle);
    if (alpm_trans_init(handle, kTransFlags) != 0) {
        logging::error(QStringLiteral("failed to create a new transaction: %1").arg(alpm_strerror(alpm_errno(handle))));
        return out;
    }
    if (!addTargetsToInstall(handle, targets)) {
        alpm_trans_release(handle);
        return out;
    }

    off_t dlsize = 0, isize = 0, rsize = 0;
    for (alpm_list_t* p = alpm_trans_get_add(handle); p; p = p->next) {
        auto* pkg = static_cast<alpm_pkg_t*>(p->data);
        dlsize += alpm_pkg_download_size(pkg);
        isize += alpm_pkg_get_isize(pkg);
        out.details += QString::fromUtf8(alpm_pkg_get_name(pkg)) + QLatin1Char('-')
            + QString::fromUtf8(alpm_pkg_get_version(pkg));
        out.details += verbose ? QLatin1Char('\n') : QLatin1Char(' ');
    }
    for (alpm_list_t* p = alpm_trans_get_remove(handle); p; p = p->next) {
        auto* pkg = static_cast<alpm_pkg_t*>(p->data);
        rsize += alpm_pkg_get_isize(pkg);
        out.details += QString::fromUtf8(alpm_pkg_get_name(pkg)) + QLatin1Char('-')
            + QString::fromUtf8(alpm_pkg_get_version(pkg)) + QStringLiteral(" [removal]");
        out.details += verbose ? QLatin1Char('\n') : QLatin1Char(' ');
    }

    if (!out.details.isEmpty()) {
        if (dlsize > 0) {
            out.statusText += QStringLiteral("Total Download Size: %1\n").arg(displaySizes(dlsize));
        }
        if (isize > 0) {
            out.statusText += QStringLiteral("Total Installed Size: %1\n").arg(displaySizes(isize));
        }
        if (rsize > 0 && isize == 0) {
            out.statusText += QStringLiteral("Total Removed Size: %1\n").arg(displaySizes(rsize));
        }
        if (isize > 0 && rsize > 0) {
            out.statusText += QStringLiteral("Net Upgrade Size: %1\n").arg(displaySizes(isize - rsize));
        }
        out.details += QLatin1Char('\n');
    }
    alpm_trans_release(handle);
    return out;
}

AlpmManager::Preview AlpmManager::displayRemoveTargets(const QStringList& targets, bool verbose) {
    QMutexLocker lock(&m_mutex);
    Preview out;
    auto* handle = static_cast<alpm_handle_t*>(m_handle);
    if (alpm_trans_init(handle, kTransFlags) != 0) {
        logging::error(QStringLiteral("failed to create a new transaction: %1").arg(alpm_strerror(alpm_errno(handle))));
        return out;
    }
    if (!addTargetsToRemove(handle, targets)) {
        alpm_trans_release(handle);
        return out;
    }
    off_t isize = 0, rsize = 0;
    for (alpm_list_t* p = alpm_trans_get_add(handle); p; p = p->next) {
        auto* pkg = static_cast<alpm_pkg_t*>(p->data);
        isize += alpm_pkg_get_isize(pkg);
        out.details += QString::fromUtf8(alpm_pkg_get_name(pkg)) + QLatin1Char('-')
            + QString::fromUtf8(alpm_pkg_get_version(pkg));
        out.details += verbose ? QLatin1Char('\n') : QLatin1Char(' ');
    }
    for (alpm_list_t* p = alpm_trans_get_remove(handle); p; p = p->next) {
        auto* pkg = static_cast<alpm_pkg_t*>(p->data);
        rsize += alpm_pkg_get_isize(pkg);
        out.details += QString::fromUtf8(alpm_pkg_get_name(pkg)) + QLatin1Char('-')
            + QString::fromUtf8(alpm_pkg_get_version(pkg)) + QStringLiteral(" [removal]");
        out.details += verbose ? QLatin1Char('\n') : QLatin1Char(' ');
    }
    if (!out.details.isEmpty()) {
        if (isize > 0) {
            out.statusText += QStringLiteral("Total Installed Size: %1\n").arg(displaySizes(isize));
        }
        if (rsize > 0 && isize == 0) {
            out.statusText += QStringLiteral("Total Removed Size: %1\n").arg(displaySizes(rsize));
        }
        if (isize > 0 && rsize > 0) {
            out.statusText += QStringLiteral("Net Upgrade Size: %1\n").arg(displaySizes(isize - rsize));
        }
        out.details += QLatin1Char('\n');
    }
    alpm_trans_release(handle);
    return out;
}

namespace {

/// Mirror of reference `print_broken_dep` warning text.
void printBrokenDep(const QString& causingPkg, const QString& pkgVersion, const alpm_depmissing_t* miss) {
    const QString depstring = QString::fromUtf8(alpm_dep_compute_string(miss->depend));
    const QString target = QString::fromUtf8(miss->target);
    if (!miss->causingpkg) {
        logging::warn(QStringLiteral("unable to satisfy dependency '%1' required by %2").arg(depstring, target));
    } else {
        const QString causing = QString::fromUtf8(miss->causingpkg);
        if (!causingPkg.isEmpty() && causing == causingPkg) {
            logging::warn(QStringLiteral("installing %1 (%2) breaks dependency '%3' required by %4")
                              .arg(causing, pkgVersion, depstring, target));
        } else {
            logging::warn(QStringLiteral("removing %1 breaks dependency '%2' required by %3")
                              .arg(causing, depstring, target));
        }
    }
}

}  // namespace

int AlpmManager::prepareAddTrans(const QStringList& targets, QString& conflictMessage) {
    QMutexLocker lock(&m_mutex);
    auto* handle = static_cast<alpm_handle_t*>(m_handle);
    if (alpm_trans_init(handle, kTransFlags) != 0) {
        logging::error(QStringLiteral("failed to create a new transaction: %1").arg(alpm_strerror(alpm_errno(handle))));
        return 1;
    }
    if (!addTargetsToInstall(handle, targets)) {
        alpm_trans_release(handle);
        return 1;
    }

    // collect explicit add list for broken-dep reporting (reference parity)
    QMap<QString, QString> addVersions;
    for (alpm_list_t* p = alpm_trans_get_add(handle); p; p = p->next) {
        auto* pkg = static_cast<alpm_pkg_t*>(p->data);
        addVersions.insert(QString::fromUtf8(alpm_pkg_get_name(pkg)), QString::fromUtf8(alpm_pkg_get_version(pkg)));
    }

    if (alpm_trans_get_add(handle) == nullptr) {
        // nothing to do: exit without complaining (reference parity)
        alpm_trans_release(handle);
        return 0;
    }

    alpm_list_t* data = nullptr;
    if (alpm_trans_prepare(handle, &data) != 0) {
        const int err = alpm_errno(handle);
        logging::error(QStringLiteral("failed to prepare transaction (%1)").arg(alpm_strerror(static_cast<alpm_errno_t>(err))));
        switch (err) {
        case ALPM_ERR_PKG_INVALID_ARCH:
            // data entries are "name-version-arch" strings (libalpm formats
            // them in check_arch)
            for (alpm_list_t* p = data; p; p = p->next) {
                logging::info(QStringLiteral("package %1 does not have a valid architecture")
                                  .arg(QString::fromUtf8(static_cast<char*>(p->data))));
            }
            break;
        case ALPM_ERR_UNSATISFIED_DEPS:
            for (alpm_list_t* p = data; p; p = p->next) {
                auto* miss = static_cast<alpm_depmissing_t*>(p->data);
                QString causing = miss->causingpkg ? QString::fromUtf8(miss->causingpkg) : QString();
                printBrokenDep(causing, addVersions.value(causing), miss);
            }
            break;
        case ALPM_ERR_CONFLICTING_DEPS:
            for (alpm_list_t* p = data; p; p = p->next) {
                auto* conflict = static_cast<alpm_conflict_t*>(p->data);
                const QString pkg1 = QString::fromUtf8(alpm_pkg_get_name(conflict->package1));
                const QString pkg2 = QString::fromUtf8(alpm_pkg_get_name(conflict->package2));
                if (conflict->reason && conflict->reason->mod != ALPM_DEP_MOD_ANY) {
                    conflictMessage += QStringLiteral("'%1' and '%2' are in conflict (%3)\n")
                                           .arg(pkg1, pkg2, QString::fromUtf8(alpm_dep_compute_string(conflict->reason)));
                    logging::info(QStringLiteral("'%1' and '%2' are in conflict (%3)")
                                      .arg(pkg1, pkg2, QString::fromUtf8(alpm_dep_compute_string(conflict->reason))));
                } else {
                    conflictMessage += QStringLiteral("'%1' and '%2' are in conflict\n").arg(pkg1, pkg2);
                    logging::info(QStringLiteral("'%1' and '%2' are in conflict").arg(pkg1, pkg2));
                }
            }
            break;
        default:
            logging::info(QStringLiteral("err data invalid"));
            break;
        }
        alpm_list_free(data);
    }

    alpm_trans_release(handle);
    return 0;
}

std::optional<alpm::PackageView> AlpmManager::getPackageView(const QString& name) {
    QMutexLocker lock(&m_mutex);
    auto* handle = static_cast<alpm_handle_t*>(m_handle);
    for (alpm_list_t* d = alpm_get_syncdbs(handle); d; d = d->next) {
        alpm_pkg_t* pkg = alpm_db_get_pkg(static_cast<alpm_db_t*>(d->data), name.toUtf8().constData());
        if (pkg) {
            return PackageView{QString::fromUtf8(alpm_pkg_get_name(pkg)),
                QString::fromUtf8(alpm_pkg_get_version(pkg)),
                alpm_pkg_get_desc(pkg) ? QString::fromUtf8(alpm_pkg_get_desc(pkg)) : QString(), false};
        }
    }
    return std::nullopt;
}

std::optional<alpm::AlpmManager::PackageInfo> AlpmManager::getPackageInfo(const QString& name) {
    QMutexLocker lock(&m_mutex);
    auto* handle = static_cast<alpm_handle_t*>(m_handle);
    for (alpm_list_t* d = alpm_get_syncdbs(handle); d; d = d->next) {
        auto* db = static_cast<alpm_db_t*>(d->data);
        alpm_pkg_t* pkg = alpm_db_get_pkg(db, name.toUtf8().constData());
        if (!pkg) {
            continue;
        }
        const auto toList = [](alpm_list_t* list) {
            QStringList out;
            for (alpm_list_t* p = list; p; p = p->next) {
                out << QString::fromUtf8(static_cast<char*>(p->data));
            }
            return out;
        };
        PackageInfo info;
        info.name = QString::fromUtf8(alpm_pkg_get_name(pkg));
        info.version = QString::fromUtf8(alpm_pkg_get_version(pkg));
        info.desc = alpm_pkg_get_desc(pkg) ? QString::fromUtf8(alpm_pkg_get_desc(pkg)) : QString();
        info.url = alpm_pkg_get_url(pkg) ? QString::fromUtf8(alpm_pkg_get_url(pkg)) : QString();
        info.arch = alpm_pkg_get_arch(pkg) ? QString::fromUtf8(alpm_pkg_get_arch(pkg)) : QString();
        info.packager = alpm_pkg_get_packager(pkg) ? QString::fromUtf8(alpm_pkg_get_packager(pkg)) : QString();
        info.licenses = toList(alpm_pkg_get_licenses(pkg));
        info.groups = toList(alpm_pkg_get_groups(pkg));
        info.depends = toList(alpm_pkg_get_depends(pkg));
        info.optDepends = toList(alpm_pkg_get_optdepends(pkg));
        info.conflicts = toList(alpm_pkg_get_conflicts(pkg));
        info.provides = toList(alpm_pkg_get_provides(pkg));
        info.repo = QString::fromUtf8(alpm_db_get_name(db));
        info.downloadSize = static_cast<qint64>(alpm_pkg_download_size(pkg));
        info.installedSize = static_cast<qint64>(alpm_pkg_get_isize(pkg));
        switch (alpm_pkg_get_validation(pkg)) {
        case ALPM_PKG_VALIDATION_NONE: info.validation = QCoreApplication::translate("PackageInfo", "None"); break;
        case ALPM_PKG_VALIDATION_MD5SUM: info.validation = QCoreApplication::translate("PackageInfo", "MD5"); break;
        case ALPM_PKG_VALIDATION_SHA256SUM: info.validation = QCoreApplication::translate("PackageInfo", "SHA-256"); break;
        case ALPM_PKG_VALIDATION_SIGNATURE: info.validation = QCoreApplication::translate("PackageInfo", "Signature"); break;
        default: info.validation = QCoreApplication::translate("PackageInfo", "Unknown"); break;
        }
        if (alpm_pkg_t* lp = alpm_db_get_pkg(alpm_get_localdb(handle), name.toUtf8().constData())) {
            switch (alpm_pkg_get_reason(lp)) {
            case ALPM_PKG_REASON_EXPLICIT: info.installReason = QCoreApplication::translate("PackageInfo", "Explicitly installed"); break;
            case ALPM_PKG_REASON_DEPEND: info.installReason = QCoreApplication::translate("PackageInfo", "Installed as a dependency"); break;
            default: break;
            }
        }
        return info;
    }
    return std::nullopt;
}

std::vector<PackageView> AlpmManager::getListOfPackages() {
    QMutexLocker lock(&m_mutex);
    std::vector<PackageView> result;
    auto* handle = static_cast<alpm_handle_t*>(m_handle);
    QSet<QString> seen;
    

    alpm_db_t* local = alpm_get_localdb(handle);
    for (alpm_list_t* d = alpm_get_syncdbs(handle); d; d = d->next) {
        auto* db = static_cast<alpm_db_t*>(d->data);
        for (alpm_list_t* p = alpm_db_get_pkgcache(db); p; p = p->next) {
            auto* pkg = static_cast<alpm_pkg_t*>(p->data);
            const QString name = QString::fromUtf8(alpm_pkg_get_name(pkg));
            if (seen.contains(name)) {
                continue;  // first (repo-priority) occurrence wins, like pacman
            }
            seen.insert(name);
            const QString ver = QString::fromUtf8(alpm_pkg_get_version(pkg));
            bool upgradable = false;
            if (alpm_pkg_t* lp = alpm_db_get_pkg(local, name.toUtf8().constData())) {
                upgradable = alpm_pkg_vercmp(alpm_pkg_get_version(lp), ver.toUtf8().constData()) < 0;
            }
            result.push_back(PackageView{name, ver,
                alpm_pkg_get_desc(pkg) ? QString::fromUtf8(alpm_pkg_get_desc(pkg)) : QString(), upgradable});
        }
    }
    return result;
}

bool AlpmManager::isValidAlpmDbs() {
    auto manager = init();
    if (!manager) {
        return false;
    }
    QMutexLocker lock(&manager->m_mutex);
    auto* handle = static_cast<alpm_handle_t*>(manager->m_handle);
    size_t count = 0;
    for (alpm_list_t* d = alpm_get_syncdbs(handle); d; d = d->next) {
        count += alpm_list_count(alpm_db_get_pkgcache(static_cast<alpm_db_t*>(d->data)));
    }
    return count != 0;
}

QStringList AlpmManager::listInstalledNames() {
    QMutexLocker lock(&m_mutex);
    auto* handle = static_cast<alpm_handle_t*>(m_handle);
    QStringList out;
    for (alpm_list_t* p = alpm_db_get_pkgcache(alpm_get_localdb(handle)); p; p = p->next) {
        out << QString::fromUtf8(alpm_pkg_get_name(static_cast<alpm_pkg_t*>(p->data)));
    }
    return out;
}

QMap<QString, QString> AlpmManager::listInstalledVersions() {
    QMutexLocker lock(&m_mutex);
    auto* handle = static_cast<alpm_handle_t*>(m_handle);
    QMap<QString, QString> out;
    for (alpm_list_t* p = alpm_db_get_pkgcache(alpm_get_localdb(handle)); p; p = p->next) {
        auto* pkg = static_cast<alpm_pkg_t*>(p->data);
        out.insert(QString::fromUtf8(alpm_pkg_get_name(pkg)), QString::fromUtf8(alpm_pkg_get_version(pkg)));
    }
    return out;
}

}  // namespace alpm
