#pragma once

#include "catalogmodel.hpp"
#include "flatpakparser.hpp"
#include "operationcommands.hpp"
#include "processrunner.hpp"
#include "repositoryworker.hpp"

#include <QNetworkAccessManager>
#include <QThread>
#include <QVariantMap>

class AppController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(CatalogModel* popularModel READ popularModel CONSTANT)
    Q_PROPERTY(CatalogModel* repositoryModel READ repositoryModel CONSTANT)
    Q_PROPERTY(CatalogModel* flatpakModel READ flatpakModel CONSTANT)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(bool operationRunning READ operationRunning NOTIFY operationRunningChanged)
    Q_PROPERTY(bool transactionVisible READ transactionVisible NOTIFY transactionChanged)
    Q_PROPERTY(QVariantMap transactionPreview READ transactionPreview NOTIFY transactionChanged)
    Q_PROPERTY(QString operationTitle READ operationTitle NOTIFY operationRunningChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString consoleText READ consoleText NOTIFY consoleChanged)
    Q_PROPERTY(bool resultVisible READ resultVisible NOTIFY resultChanged)
    Q_PROPERTY(bool resultSuccess READ resultSuccess NOTIFY resultChanged)
    Q_PROPERTY(QString resultMessage READ resultMessage NOTIFY resultChanged)
    Q_PROPERTY(QString fatalError READ fatalError NOTIFY fatalErrorChanged)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(QString page READ page WRITE setPage NOTIFY pageChanged)
    Q_PROPERTY(bool flatpakAvailable READ flatpakAvailable NOTIFY flatpakStateChanged)
    Q_PROPERTY(bool flatpakLoading READ flatpakLoading NOTIFY flatpakStateChanged)
    Q_PROPERTY(QString flatpakScope READ flatpakScope WRITE setFlatpakScope NOTIFY flatpakStateChanged)
    Q_PROPERTY(QString selectedRemote READ selectedRemote WRITE setSelectedRemote NOTIFY flatpakStateChanged)
    Q_PROPERTY(QString selectedRemoteUrl READ selectedRemoteUrl NOTIFY flatpakStateChanged)
    Q_PROPERTY(QStringList remoteNames READ remoteNames NOTIFY flatpakStateChanged)
    Q_PROPERTY(bool showFlatpak READ showFlatpak WRITE setShowFlatpak NOTIFY flatpakStateChanged)
    Q_PROPERTY(bool hideLibraries READ hideLibraries WRITE setHideLibraries NOTIFY settingsChanged)
    Q_PROPERTY(bool flatpakWarningVisible READ flatpakWarningVisible NOTIFY flatpakWarningChanged)
    Q_PROPERTY(int windowWidth READ windowWidth CONSTANT)
    Q_PROPERTY(int windowHeight READ windowHeight CONSTANT)
    Q_PROPERTY(bool systemReady READ systemReady NOTIFY systemReadyChanged)

public:
    explicit AppController(QObject* parent = nullptr);
    ~AppController() override;

    CatalogModel* popularModel() { return &m_popularModel; }
    CatalogModel* repositoryModel() { return &m_repositoryModel; }
    CatalogModel* flatpakModel() { return &m_flatpakModel; }
    bool loading() const { return m_loading; }
    bool ready() const { return m_ready; }
    bool operationRunning() const { return m_operationRunning; }
    bool transactionVisible() const { return m_transactionVisible; }
    QVariantMap transactionPreview() const { return m_transactionMap; }
    QString operationTitle() const { return m_operationTitle; }
    QString statusMessage() const { return m_statusMessage; }
    QString consoleText() const { return m_consoleText; }
    bool resultVisible() const { return m_resultVisible; }
    bool resultSuccess() const { return m_resultSuccess; }
    QString resultMessage() const { return m_resultMessage; }
    QString fatalError() const { return m_fatalError; }
    QString theme() const { return m_theme; }
    QString page() const { return m_page; }
    bool flatpakAvailable() const { return m_flatpakAvailable; }
    bool flatpakLoading() const { return m_flatpakLoading; }
    QString flatpakScope() const { return m_flatpakScope; }
    QString selectedRemote() const { return m_selectedRemote; }
    QString selectedRemoteUrl() const;
    QStringList remoteNames() const { return m_remoteNames; }
    bool showFlatpak() const { return m_showFlatpak; }
    bool hideLibraries() const { return m_hideLibraries; }
    bool flatpakWarningVisible() const { return m_flatpakWarningVisible; }
    int windowWidth() const { return m_windowWidth; }
    int windowHeight() const { return m_windowHeight; }
    bool systemReady() const { return m_systemReady; }

    void setTheme(const QString& value);
    void setPage(const QString& value);
    void setFlatpakScope(const QString& value);
    void setSelectedRemote(const QString& value);
    void setShowFlatpak(bool value);
    void setHideLibraries(bool value);

    Q_INVOKABLE void refreshRepositories();
    Q_INVOKABLE void installSelected(const QString& source);
    Q_INVOKABLE void removeSelected(const QString& source);
    Q_INVOKABLE void upgradeSelected();
    Q_INVOKABLE void upgradeAll();
    Q_INVOKABLE void removeOrphans();
    Q_INVOKABLE void confirmTransaction();
    Q_INVOKABLE void cancelTransaction();
    Q_INVOKABLE void dismissTransaction();
    Q_INVOKABLE void dismissResult();
    Q_INVOKABLE void sendConsoleInput(const QString& text);
    Q_INVOKABLE void loadFlatpaks();
    Q_INVOKABLE void installFlatpakSupport();
    Q_INVOKABLE void refreshFlatpakMetadata();
    Q_INVOKABLE void updateFlatpaks();
    Q_INVOKABLE void updateSelectedFlatpaks();
    Q_INVOKABLE void removeUnusedFlatpaks();
    Q_INVOKABLE void addRemote(const QString& name, const QString& url);
    Q_INVOKABLE void removeRemote(const QString& name);
    Q_INVOKABLE void installFlatpakref(const QString& location);
    Q_INVOKABLE void dismissFlatpakWarning(bool disable);
    Q_INVOKABLE void saveWindowSize(int width, int height);
    Q_INVOKABLE void openHelp();
    Q_INVOKABLE void retry();

signals:
    void loadingChanged();
    void readyChanged();
    void operationRunningChanged();
    void transactionChanged();
    void statusMessageChanged();
    void consoleChanged();
    void resultChanged();
    void fatalErrorChanged();
    void themeChanged();
    void pageChanged();
    void flatpakStateChanged();
    void flatpakWarningChanged();
    void systemReadyChanged();
    void settingsChanged();

private slots:
    void onSnapshotReady(const PackageSnapshot& snapshot);
    void onPreviewReady(const TransactionPreview& preview);
    void onWorkerError(const QString& error);
    void onProcessOutput(const QString& text);
    void onProcessError(const QString& text);
    void onProcessFinished(int exitCode, QProcess::ExitStatus status, const QString& output, bool cancelled);

private:
    enum class ProcessPurpose {
        None,
        NativeMutation,
        FlatpakRemotes,
        FlatpakInstalled,
        FlatpakApps,
        FlatpakRuntimes,
        FlatpakUpdates,
        FlatpakBootstrap,
        FlatpakMutation,
        FlatpakAppstreamRefresh,
        OrphanQuery,
    };

    void startRepositoryWorker();
    void applyCurated(const QByteArray& yaml);
    void fetchCurated();
    void rebuildPopular();
    void beginNativePreview(const QStringList& targets, const QString& action);
    void beginFlatpakPreview(const QStringList& refs, const QString& action);
    void startMutation(const QString& action, const QString& source, const QStringList& targets);
    void startProcess(const QString& program, const QStringList& arguments, ProcessPurpose purpose, const QString& title);
    void startFlatpakListing();
    QStringList scopedFlatpakArgs() const;
    void finishOperation(bool success, const QString& message);
    void setStatus(const QString& message);
    void setBusy(bool value, const QString& title = {});
    void setSystemReady(bool value);
    static bool validRemoteName(const QString& name);
    static bool validFlatpakrefLocation(const QString& location);

    CatalogModel m_popularModel;
    CatalogModel m_repositoryModel;
    CatalogModel m_flatpakModel;
    ProcessRunner m_runner;
    QNetworkAccessManager m_network;
    QThread* m_repositoryThread = nullptr;
    RepositoryWorker* m_repositoryWorker = nullptr;

    PackageSnapshot m_snapshot;
    QVector<CuratedSpec> m_curated;
    QString m_flatpakAppsOutput;
    QVector<CatalogItem> m_flatpakItemsPending;
    QStringList m_installedFlatpakRefs;
    QVector<FlatpakRemote> m_remotes;
    QStringList m_remoteNames;
    QVariantMap m_transactionMap;

    ProcessPurpose m_processPurpose = ProcessPurpose::None;
    QString m_pendingAction;
    QString m_pendingSource;
    QStringList m_pendingTargets;
    bool m_loading = true;
    bool m_ready = false;
    bool m_operationRunning = false;
    bool m_transactionVisible = false;
    bool m_flatpakAvailable = false;
    bool m_flatpakLoading = false;
    bool m_systemReady = false;
    bool m_ignorePreview = false;
    int m_bootstrapStep = 0;
    QString m_bootstrapScope;
    QString m_operationTitle;
    QString m_statusMessage;
    QString m_consoleText;
    bool m_resultVisible = false;
    bool m_resultSuccess = false;
    QString m_resultMessage;
    QString m_fatalError;
    QString m_theme = QStringLiteral("System");
    QString m_page = QStringLiteral("home");
    QString m_flatpakScope = QStringLiteral("system");
    QString m_selectedRemote;
    bool m_showFlatpak = true;
    bool m_hideLibraries = true;
    bool m_flatpakWarningVisible = false;
    bool m_flatpakWarningShown = false;
    bool m_disableWarning = false;
    int m_windowWidth = 1380;
    int m_windowHeight = 860;
};
