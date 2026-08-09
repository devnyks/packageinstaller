#pragma once

#include "alpm_manager.h"
#include "appstream_provider.h"
#include "cmd.h"
#include "flatpak.h"
#include "pkglist.h"

#include <QCloseEvent>
#include <QMainWindow>
#include <QSettings>

#include <memory>

class NavBar;
class QStackedWidget;
class DiscoverPage;
class RepoPage;
class FlatpakPage;
class ConsolePage;
class SettingsPage;
class ProgressOverlay;

/// Application shell: navigation, page orchestration and the package
/// operation controller (confirm -> console -> pkexec/flatpak -> refresh).
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    /// ---- test hooks (exercised by tests/ui_test.sh) ----
    void confirmAndInstallPublic(const QStringList& names) { confirmAndInstall(names); }
    void confirmAndUninstallPublic(const QStringList& names) { confirmAndUninstall(names); }
    void upgradeAllPublic() { upgradeAll(); }
    void removeOrphansPublic() { removeOrphans(); }
    void confirmFlatpakInstallPublic(const QStringList& ids) { confirmFlatpakInstall(ids); }
    void confirmFlatpakUninstallPublic(const QStringList& ids) { confirmFlatpakUninstall(ids); }
    void flatpakUpdateAllPublic() { flatpakUpdateAll(); }
    void flatpakRemoveUnusedPublic() { flatpakRemoveUnused(); }
    RepoPage* repoPage() const { return m_repoPage; }
    FlatpakPage* flatpakPage() const { return m_flatpakPage; }
    ConsolePage* consolePage() const { return m_console; }
    SettingsPage* settingsPage() const { return m_settings; }
    DiscoverPage* discoverPage() const { return m_discover; }
    bool opInProgress() const { return m_opInProgress; }
    QStringList installedNames() const { return m_installedNames.values(); }
    alpm::AlpmManagerPtr alpmManager() const { return m_alpm; }
    NavBar* navBar() const { return m_nav; }

private:
    // ---- startup / data ----
    void setup();
    void loadPackageDataAsync();
    void loadPopularAsync();
    void loadFlatpakAsync(bool force);
    void loadAppStream();
    void refreshAll();
    void onOperationDone(bool ok, const QString& successMessage, const QString& failureMessage,
        bool refreshRepo, bool refreshFlatpak);

    // ---- pacman operations ----
    void runPacmanOperation(const QStringList& names, const QString& title, bool remove,
        const QString& command);
    void confirmAndInstall(const QStringList& names);
    void confirmAndUninstall(const QStringList& names);
    void upgradeAll();
    void removeOrphans();
    void showPackageInfo(const QString& name);

    // ---- flatpak operations ----
    void ensureFlatpakAvailable();
    void confirmFlatpakInstall(const QStringList& appIds);
    void confirmFlatpakUninstall(const QStringList& appIds);
    void runNextPendingCommand();
    void flatpakUpdateAll();
    void flatpakRemoveUnused();
    void manageRemotes();
    void installFlatpakRef(const QString& ref, bool userScope);
    void onFlatpakScopeChanged(bool userScope);
    void onFlatpakRemoteChanged(const QString& remote);
    void showFlatpakWarning();
    void bootstrapFlathubRemotes();

    // ---- navigation (public: also used by tests) ----
public:
    void navigate(int index);
    void switchConsole(const QString& title);
    void buildPopular();

    /// Public data refresh hook for tests.
    void refreshRepoData() { refreshAll(); }

private:

protected:
    void closeEvent(QCloseEvent* event) override;

    bool m_flatpakInstalled() const;

    NavBar* m_nav = nullptr;
    QStackedWidget* m_stack = nullptr;
    DiscoverPage* m_discover = nullptr;
    RepoPage* m_repoPage = nullptr;
    FlatpakPage* m_flatpakPage = nullptr;
    ConsolePage* m_console = nullptr;
    SettingsPage* m_settings = nullptr;
    ProgressOverlay* m_progress = nullptr;

    Cmd m_cmd;
    FlatpakBackend m_flatpak;
    AppStreamProvider m_appstream;

    alpm::AlpmManagerPtr m_alpm;
    QSettings m_settingsStore;

    // package data snapshot (worker-produced, UI-read)
    QMap<QString, QStringList> m_candidates;      // name -> [version, desc]
    QStringList m_upgradable;
    QMap<QString, QString> m_installedVersions;   // name -> version
    QSet<QString> m_installedNames;
    QList<PopularEntry> m_popularEntries;
    bool m_popularLoaded = false;
    bool m_repoLoaded = false;

    bool m_opInProgress = false;
    bool m_flatpakLoaded = false;
    bool m_flatpakFirstList = true;
    bool m_bootstrapFlatpak = false;
    QStringList m_pendingCommands;
    QString m_flatpakScope = QStringLiteral("--system ");
    QString m_lastCommand;
    bool m_quitConfirmed = false;
};
