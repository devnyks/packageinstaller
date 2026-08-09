#pragma once

#include <QMap>
#include <QMutex>
#include <QString>
#include <QStringList>

#include <memory>
#include <optional>
#include <string>
#include <vector>

/// ALPM (libalpm) wrapper replicating the reference Rust backend semantics:
///  - handle initialized from /etc/pacman.conf,
///  - refresh recreates the handle from config,
///  - transactions are always DB_ONLY | ALL_DEPS | ALL_EXPLICIT | NO_LOCK
///    (read-only previews; the real operations run through `pkexec pacman`),
///  - conflict/unsatisfied-dependency reporting identical to the reference.
///
/// The handle is not thread-safe; the UI serializes access through
/// AlpmManager's internal mutex.
namespace alpm {

struct PackageView {
    QString name;
    QString version;
    QString desc;
    bool upgradable = false;
};

class AlpmManager {
public:
    static std::optional<AlpmManager> init();

    void refresh();

    /// Preview the targets that an install of `targets` would affect.
    /// Returns package/group/repo-qualified target resolution and
    /// size summary text (reference `display_install_targets`).
    struct Preview {
        QString details;    // "name-version" lines
        QString statusText; // "Total Download Size: ..." lines
    };
    Preview displayInstallTargets(const QStringList& targets, bool verbose = false);
    Preview displayRemoveTargets(const QStringList& targets, bool verbose = false);

    /// Prepare install transaction to detect conflicts. Fills
    /// `conflictMessage`; returns 0 on success (no hard conflicts), 1 on error
    /// or when conflicts were found (reference `prepare_add_trans`).
    int prepareAddTrans(const QStringList& targets, QString& conflictMessage);

    std::optional<PackageView> getPackageView(const QString& name);

    /// Full package information for the info dialog (reference `pacman -Si`
    /// fields, read directly from the sync databases).
    struct PackageInfo {
        QString name;
        QString version;
        QString desc;
        QString url;
        QString arch;
        QString packager;
        QStringList licenses;
        QStringList groups;
        QStringList depends;
        QStringList optDepends;
        QStringList conflicts;
        QStringList provides;
        QString repo;
        qint64 downloadSize = 0;
        qint64 installedSize = 0;
        QString installReason;
        QString validation;
    };
    std::optional<PackageInfo> getPackageInfo(const QString& name);

    /// Full sync-db package list, first-repo occurrence wins, with
    /// upgradable flags computed against the local db (reference
    /// `get_list_of_packages` + `pacman_cache`).
    std::vector<PackageView> getListOfPackages();

    /// Whether any sync db contains packages (reference `is_valid_alpm_dbs`).
    static bool isValidAlpmDbs();

    /// Local database package names (fast `pacman -Qq` equivalent).
    QStringList listInstalledNames();

    /// name -> installed version (fast `pacman -Q` equivalent).
    QMap<QString, QString> listInstalledVersions();

private:
    explicit AlpmManager(void* handle) : m_handle(handle) {}
    void setupCallbacks();
    void registerSyncDbs();

    void* m_handle = nullptr;  // alpm_handle_t*
    mutable QMutex m_mutex;

public:
    ~AlpmManager();
    AlpmManager(const AlpmManager&) = delete;
    AlpmManager& operator=(const AlpmManager&) = delete;
    AlpmManager(AlpmManager&& other) noexcept;
    AlpmManager& operator=(AlpmManager&& other) noexcept;
};

using AlpmManagerPtr = std::shared_ptr<AlpmManager>;

}  // namespace alpm
