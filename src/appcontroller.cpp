#include "appcontroller.hpp"

#include "curatedcatalog.hpp"

#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

#include <algorithm>

namespace {

QVariantList variantList(const QStringList& values) {
    QVariantList result;
    result.reserve(values.size());
    for (const auto& value : values) {
        result << value;
    }
    return result;
}

QStringList uniqueTargets(const QVector<CatalogItem>& items, bool flatpak) {
    QStringList result;
    for (const auto& item : items) {
        const auto targets = item.targets.isEmpty() ? QStringList{item.name} : item.targets;
        for (const auto& target : targets) {
            if (!target.isEmpty() && !result.contains(target)) {
                result << (flatpak ? item.ref : target);
            }
        }
    }
    return result;
}

QString pacmanLockPath() {
    QProcess process;
    process.setProgram(QStringLiteral("pacman-conf"));
    process.setArguments({QStringLiteral("DBPath")});
    process.start();
    if (process.waitForStarted(500) && process.waitForFinished(1000)
        && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0) {
        for (const auto& line : QString::fromLocal8Bit(process.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts)) {
            if (line.trimmed().startsWith(QStringLiteral("DBPath"))) {
                const auto path = line.section('=', 1).trimmed();
                if (!path.isEmpty()) {
                    return QDir::cleanPath(QDir(path).filePath(QStringLiteral("db.lck")));
                }
            }
        }
    }
    return QStringLiteral("/var/lib/pacman/db.lck");
}

} // namespace

AppController::AppController(QObject* parent)
    : QObject(parent),
      m_popularModel(this),
      m_repositoryModel(this),
      m_flatpakModel(this),
      m_runner(this),
      m_network(this) {
    qRegisterMetaType<PackageSnapshot>();
    qRegisterMetaType<TransactionPreview>();
    qRegisterMetaType<QVector<FlatpakRemote>>();

    QSettings settings;
    m_theme = settings.value(QStringLiteral("theme"), QStringLiteral("System")).toString();
    const auto environmentTheme = qEnvironmentVariable("CACHYOS_CATALOG_THEME");
    if (environmentTheme == QStringLiteral("Light") || environmentTheme == QStringLiteral("Dark") || environmentTheme == QStringLiteral("System")) {
        m_theme = environmentTheme;
    }
    m_page = settings.value(QStringLiteral("page"), QStringLiteral("home")).toString();
    if (m_page != QStringLiteral("home") && m_page != QStringLiteral("native") && m_page != QStringLiteral("flatpak") && m_page != QStringLiteral("settings")) {
        m_page = QStringLiteral("home");
    }
    m_flatpakScope = settings.value(QStringLiteral("flatpakScope"), QStringLiteral("system")).toString();
    m_selectedRemote = settings.value(QStringLiteral("flatpakRemote")).toString();
    m_showFlatpak = settings.value(QStringLiteral("showFlatpak"), true).toBool();
    m_hideLibraries = settings.value(QStringLiteral("hideLibraries"), true).toBool();
    m_repositoryModel.setHideLibraries(m_hideLibraries);
    m_disableWarning = settings.value(QStringLiteral("disableWarning"), false).toBool();
    m_windowWidth = settings.value(QStringLiteral("windowWidth"), 1380).toInt();
    m_windowHeight = settings.value(QStringLiteral("windowHeight"), 860).toInt();
    m_flatpakAvailable = !QStandardPaths::findExecutable(QStringLiteral("flatpak")).isEmpty();

    connect(&m_runner, &ProcessRunner::outputAvailable, this, &AppController::onProcessOutput);
    connect(&m_runner, &ProcessRunner::errorAvailable, this, &AppController::onProcessError);
    connect(&m_runner, &ProcessRunner::completed, this, &AppController::onProcessFinished);

    connect(&m_popularModel, &CatalogModel::selectionChanged, this, [this] { emit transactionChanged(); });
    connect(&m_repositoryModel, &CatalogModel::selectionChanged, this, [this] { emit transactionChanged(); });
    connect(&m_flatpakModel, &CatalogModel::selectionChanged, this, [this] { emit transactionChanged(); });

    startRepositoryWorker();
    fetchCurated();
}

AppController::~AppController() {
    QSettings settings;
    settings.setValue(QStringLiteral("theme"), m_theme);
    settings.setValue(QStringLiteral("page"), m_page);
    settings.setValue(QStringLiteral("flatpakScope"), m_flatpakScope);
    settings.setValue(QStringLiteral("flatpakRemote"), m_selectedRemote);
    settings.setValue(QStringLiteral("showFlatpak"), m_showFlatpak);
    settings.setValue(QStringLiteral("hideLibraries"), m_hideLibraries);
    settings.setValue(QStringLiteral("disableWarning"), m_disableWarning);

    m_runner.cancel();
    if (m_repositoryThread) {
        m_repositoryThread->quit();
        m_repositoryThread->wait(5000);
    }
}

void AppController::setTheme(const QString& value) {
    const auto normalized = value == QStringLiteral("Light") || value == QStringLiteral("Dark") ? value : QStringLiteral("System");
    if (m_theme == normalized) {
        return;
    }
    m_theme = normalized;
    QSettings().setValue(QStringLiteral("theme"), m_theme);
    emit themeChanged();
}

void AppController::setPage(const QString& value) {
    if (value.isEmpty() || m_page == value) {
        return;
    }
    m_page = value;
    QSettings().setValue(QStringLiteral("page"), m_page);
    emit pageChanged();
}

void AppController::setFlatpakScope(const QString& value) {
    const auto normalized = value == QStringLiteral("user") ? QStringLiteral("user") : QStringLiteral("system");
    if (m_flatpakScope == normalized) {
        return;
    }
    m_flatpakScope = normalized;
    QSettings().setValue(QStringLiteral("flatpakScope"), m_flatpakScope);
    emit flatpakStateChanged();
    if (m_page == QStringLiteral("flatpak")) {
        loadFlatpaks();
    }
}

void AppController::setSelectedRemote(const QString& value) {
    if (m_selectedRemote == value) {
        return;
    }
    m_selectedRemote = value;
    QSettings().setValue(QStringLiteral("flatpakRemote"), m_selectedRemote);
    emit flatpakStateChanged();
    if (m_page == QStringLiteral("flatpak") && !m_selectedRemote.isEmpty()) {
        loadFlatpaks();
    }
}

QString AppController::selectedRemoteUrl() const {
    for (const auto& remote : m_remotes) {
        if (remote.name == m_selectedRemote) {
            return remote.url;
        }
    }
    return {};
}

void AppController::setShowFlatpak(bool value) {
    if (m_showFlatpak == value) {
        return;
    }
    m_showFlatpak = value;
    QSettings().setValue(QStringLiteral("showFlatpak"), m_showFlatpak);
    emit flatpakStateChanged();
}

void AppController::setHideLibraries(bool value) {
    if (m_hideLibraries == value) {
        return;
    }
    m_hideLibraries = value;
    m_repositoryModel.setHideLibraries(value);
    QSettings().setValue(QStringLiteral("hideLibraries"), value);
    emit settingsChanged();
}

void AppController::dismissFlatpakWarning(bool disable) {
    m_flatpakWarningVisible = false;
    m_disableWarning = m_disableWarning || disable;
    QSettings settings;
    settings.setValue(QStringLiteral("disableWarning"), m_disableWarning);
    emit flatpakWarningChanged();
}

void AppController::saveWindowSize(int width, int height) {
    if (width < 640 || height < 480) {
        return;
    }
    m_windowWidth = width;
    m_windowHeight = height;
    QSettings settings;
    settings.setValue(QStringLiteral("windowWidth"), width);
    settings.setValue(QStringLiteral("windowHeight"), height);
}

void AppController::startRepositoryWorker() {
    m_repositoryThread = new QThread(this);
    m_repositoryWorker = new RepositoryWorker();
    m_repositoryWorker->moveToThread(m_repositoryThread);
    connect(m_repositoryThread, &QThread::finished, m_repositoryWorker, &QObject::deleteLater);
    connect(m_repositoryWorker, &RepositoryWorker::snapshotReady, this, &AppController::onSnapshotReady, Qt::QueuedConnection);
    connect(m_repositoryWorker, &RepositoryWorker::previewReady, this, &AppController::onPreviewReady, Qt::QueuedConnection);
    connect(m_repositoryWorker, &RepositoryWorker::fatalError, this, &AppController::onWorkerError, Qt::QueuedConnection);
    m_repositoryThread->start();
    QMetaObject::invokeMethod(m_repositoryWorker, &RepositoryWorker::load, Qt::QueuedConnection);
}

void AppController::refreshRepositories() {
    if (m_loading || m_operationRunning || !m_repositoryWorker) {
        return;
    }
    m_repositoryModel.clearSelection();
    m_loading = true;
    m_fatalError.clear();
    emit loadingChanged();
    emit fatalErrorChanged();
    setStatus(tr("Refreshing package metadata..."));
    QMetaObject::invokeMethod(m_repositoryWorker, &RepositoryWorker::load, Qt::QueuedConnection);
}

void AppController::onSnapshotReady(const PackageSnapshot& snapshot) {
    m_snapshot = snapshot;
    m_repositoryModel.setItems(snapshot.packages);
    rebuildPopular();
    setSystemReady(true);
    m_loading = false;
    m_ready = true;
    emit loadingChanged();
    emit readyChanged();
    setStatus(tr("%1 native packages ready").arg(snapshot.packages.size()));
}

void AppController::onWorkerError(const QString& error) {
    setSystemReady(false);
    m_ready = false;
    m_loading = false;
    m_fatalError = error;
    emit loadingChanged();
    emit fatalErrorChanged();
    setStatus(error);
}

void AppController::fetchCurated() {
    bool loaded = false;
    const auto loadFile = [this, &loaded](const QString& path) {
        if (loaded) {
            return;
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return;
        }
        const auto parsed = CuratedCatalog::parse(file.readAll());
        if (!parsed.isEmpty()) {
            m_curated = parsed;
            rebuildPopular();
            loaded = true;
        }
    };
    const auto configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    loadFile(QDir(configPath).filePath(QStringLiteral("pkglist.yaml")));
    loadFile(QStringLiteral(":/data/pkglist.yaml"));
    loadFile(QStringLiteral(":/data/resources/pkglist.yaml"));
    loadFile(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../lib/cachyos-catalog/pkglist.yaml")));
    loadFile(QStringLiteral("/usr/lib/cachyos-catalog/pkglist.yaml"));
    if (qEnvironmentVariableIsSet("CACHYOS_CATALOG_OFFLINE")) {
        setStatus(tr("Using the local curated catalogue (offline mode)"));
        return;
    }

    QNetworkRequest request(QUrl(QStringLiteral("https://raw.githubusercontent.com/cachyos/packageinstaller/develop/pkglist.yaml")));
    request.setTransferTimeout(8000);
    auto* reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        const auto contents = reply->readAll();
        if (reply->error() == QNetworkReply::NoError && !contents.isEmpty()) {
            const auto path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
            QDir().mkpath(path);
            QFile cache(QDir(path).filePath(QStringLiteral("pkglist.yaml")));
            if (cache.open(QIODevice::WriteOnly)) {
                cache.write(contents);
            }
            applyCurated(contents);
        } else if (m_curated.isEmpty()) {
            setStatus(tr("Unable to fetch the curated catalogue"));
        } else {
            setStatus(tr("Using the local curated catalogue"));
        }
        reply->deleteLater();
    });
}

void AppController::applyCurated(const QByteArray& yaml) {
    const auto parsed = CuratedCatalog::parse(yaml);
    if (parsed.isEmpty()) {
        return;
    }
    m_curated = parsed;
    rebuildPopular();
}

void AppController::rebuildPopular() {
    if (m_snapshot.packages.isEmpty() || m_curated.isEmpty()) {
        return;
    }
    QHash<QString, CatalogItem> packages;
    for (const auto& package : m_snapshot.packages) {
        packages.insert(package.id.mid(QStringLiteral("native:").size()), package);
    }
    QSet<QString> installed;
    for (const auto& package : m_snapshot.installed) {
        installed.insert(package);
    }

    QVector<CatalogItem> items;
    items.reserve(m_curated.size());
    for (const auto& spec : m_curated) {
        if (spec.targets.isEmpty()) {
            continue;
        }
        const auto primary = packages.value(spec.targets.first());
        CatalogItem item;
        item.id = QStringLiteral("curated:") + spec.category + ':' + spec.targets.join('+');
        item.name = primary.name.isEmpty() ? spec.targets.first() : primary.name;
        item.summary = primary.summary.isEmpty() ? tr("Curated application") : primary.summary;
        item.description = primary.description;
        item.category = spec.category;
        item.version = primary.version;
        item.source = QStringLiteral("curated");
        item.repo = primary.repo;
        item.targets = spec.targets;
        item.iconName = primary.iconName;
        item.iconPath = primary.iconPath;
        item.iconUrl = primary.iconUrl;
        item.screenshotUrl = primary.screenshotUrl;
        item.homepage = primary.homepage;
        item.license = primary.license;
        item.installed = std::all_of(spec.targets.cbegin(), spec.targets.cend(), [&installed](const auto& target) { return installed.contains(target); });
        item.upgradable = std::any_of(spec.targets.cbegin(), spec.targets.cend(), [&packages](const auto& target) { return packages.value(target).upgradable; });
        item.status = item.installed ? (item.upgradable ? QStringLiteral("upgradable") : QStringLiteral("installed")) : primary.name.isEmpty() ? QStringLiteral("unavailable") : QStringLiteral("available");
        items.push_back(std::move(item));
    }
    m_popularModel.setItems(std::move(items));
}

void AppController::installSelected(const QString& source) {
    CatalogModel* model = source == QStringLiteral("native") ? &m_repositoryModel : source == QStringLiteral("flatpak") ? &m_flatpakModel : &m_popularModel;
    const auto selected = model->selectedItems();
    if (selected.isEmpty()) {
        setStatus(tr("Select at least one package first"));
        return;
    }
    const bool flatpak = source == QStringLiteral("flatpak");
    const auto targets = uniqueTargets(selected, flatpak);
    if (flatpak) {
        beginFlatpakPreview(targets, QStringLiteral("install"));
    } else {
        beginNativePreview(targets, QStringLiteral("install"));
    }
}

void AppController::removeSelected(const QString& source) {
    CatalogModel* model = source == QStringLiteral("native") ? &m_repositoryModel : source == QStringLiteral("flatpak") ? &m_flatpakModel : &m_popularModel;
    const auto selected = model->selectedItems();
    if (selected.isEmpty()) {
        setStatus(tr("Select at least one package first"));
        return;
    }
    const bool flatpak = source == QStringLiteral("flatpak");
    const auto targets = uniqueTargets(selected, flatpak);
    if (flatpak) {
        beginFlatpakPreview(targets, QStringLiteral("remove"));
    } else {
        beginNativePreview(targets, QStringLiteral("remove"));
    }
}

void AppController::upgradeSelected() {
    const auto selected = m_repositoryModel.selectedItems();
    if (selected.isEmpty()) {
        setStatus(tr("Select an upgradable package first"));
        return;
    }
    beginNativePreview(uniqueTargets(selected, false), QStringLiteral("upgrade"));
}

void AppController::upgradeAll() {
    if (m_operationRunning || m_loading || !m_ready || !m_repositoryWorker) {
        return;
    }
    m_pendingAction = QStringLiteral("upgrade");
    m_pendingSource = QStringLiteral("native");
    m_pendingTargets.clear();
    m_consoleText.clear();
    emit consoleChanged();
    setBusy(true, tr("Preparing full system upgrade"));
    QMetaObject::invokeMethod(m_repositoryWorker, &RepositoryWorker::previewSystemUpgrade, Qt::QueuedConnection);
}

void AppController::removeOrphans() {
    if (m_operationRunning || m_loading || !m_ready || QStandardPaths::findExecutable(QStringLiteral("pacman")).isEmpty()) {
        return;
    }
    m_pendingAction = QStringLiteral("remove-orphans");
    m_pendingSource = QStringLiteral("native");
    m_consoleText.clear();
    emit consoleChanged();
    startProcess(QStringLiteral("pacman"), {QStringLiteral("-Qdtq")}, ProcessPurpose::OrphanQuery, tr("Finding orphan packages"));
}

void AppController::beginNativePreview(const QStringList& targets, const QString& action) {
    if (targets.isEmpty() || m_operationRunning || !m_repositoryWorker || !m_ready) {
        return;
    }
    m_ignorePreview = false;
    m_pendingAction = action;
    m_pendingSource = QStringLiteral("native");
    m_pendingTargets = targets;
    m_consoleText.clear();
    emit consoleChanged();
    setBusy(true, tr("Preparing transaction"));
    if (action == QStringLiteral("remove") || action == QStringLiteral("remove-orphans")) {
        QMetaObject::invokeMethod(m_repositoryWorker, "previewRemove", Qt::QueuedConnection, Q_ARG(QStringList, targets));
    } else {
        QMetaObject::invokeMethod(m_repositoryWorker, "previewInstall", Qt::QueuedConnection, Q_ARG(QStringList, targets));
    }
}

void AppController::beginFlatpakPreview(const QStringList& refs, const QString& action) {
    if (refs.isEmpty() || m_operationRunning) {
        return;
    }
    m_pendingAction = action;
    m_pendingSource = QStringLiteral("flatpak");
    m_pendingTargets = refs;
    m_transactionMap = {
        {QStringLiteral("action"), action},
        {QStringLiteral("success"), true},
        {QStringLiteral("summary"), tr("Flatpak scope: %1\nRemote: %2").arg(m_flatpakScope, m_selectedRemote.isEmpty() ? tr("default") : m_selectedRemote)},
        {QStringLiteral("additions"), action == QStringLiteral("install") ? variantList(refs) : QVariantList()},
        {QStringLiteral("removals"), action == QStringLiteral("remove") ? variantList(refs) : QVariantList()},
        {QStringLiteral("conflicts"), QVariantList()},
    };
    m_transactionVisible = true;
    emit transactionChanged();
}

void AppController::onPreviewReady(const TransactionPreview& preview) {
    if (m_ignorePreview) {
        m_ignorePreview = false;
        return;
    }
    setBusy(false);
    m_transactionMap = {
        {QStringLiteral("action"), preview.action},
        {QStringLiteral("success"), preview.success},
        {QStringLiteral("summary"), preview.summary},
        {QStringLiteral("error"), preview.error},
        {QStringLiteral("additions"), variantList(preview.additions)},
        {QStringLiteral("removals"), variantList(preview.removals)},
        {QStringLiteral("conflicts"), variantList(preview.conflicts)},
        {QStringLiteral("downloadSize"), preview.downloadSize},
        {QStringLiteral("installedSize"), preview.installedSize},
        {QStringLiteral("removedSize"), preview.removedSize},
    };
    m_transactionVisible = true;
    emit transactionChanged();
    if (preview.success) {
        setStatus(tr("Review the transaction before continuing"));
    } else {
        setStatus(preview.error.isEmpty() ? tr("Transaction preview failed") : preview.error);
    }
}

void AppController::confirmTransaction() {
    if (!m_transactionVisible || !m_transactionMap.value(QStringLiteral("success")).toBool()) {
        return;
    }
    m_transactionVisible = false;
    emit transactionChanged();
    startMutation(m_pendingAction, m_pendingSource, m_pendingTargets);
}

void AppController::cancelTransaction() {
    if (m_operationRunning) {
        if (m_runner.running()) {
            m_runner.cancel();
        } else {
            m_ignorePreview = true;
            m_pendingTargets.clear();
            setBusy(false);
            setStatus(tr("Transaction preview cancelled"));
        }
        return;
    }
    dismissTransaction();
}

void AppController::dismissTransaction() {
    if (!m_transactionVisible) {
        return;
    }
    m_transactionVisible = false;
    m_transactionMap.clear();
    m_pendingTargets.clear();
    emit transactionChanged();
}

void AppController::dismissResult() {
    if (!m_resultVisible) {
        return;
    }
    m_resultVisible = false;
    emit resultChanged();
}

void AppController::sendConsoleInput(const QString& text) {
    if (!text.isEmpty()) {
        m_runner.writeInput(text + '\n');
        m_consoleText += QStringLiteral("> ") + text + '\n';
        emit consoleChanged();
    }
}

void AppController::startMutation(const QString& action, const QString& source, const QStringList& targets) {
    if ((source == QStringLiteral("native") || (source == QStringLiteral("flatpak") && m_flatpakScope == QStringLiteral("system")))
        && QFile::exists(pacmanLockPath())) {
        finishOperation(false, tr("Another package operation acquired the pacman lock before this action started."));
        return;
    }
    m_consoleText.clear();
    emit consoleChanged();
    if (source == QStringLiteral("native")) {
        const auto spec = OperationCommands::nativeMutation(action, targets);
        startProcess(spec.program, spec.arguments, ProcessPurpose::NativeMutation, tr("Applying native package changes"));
        return;
    }
    const auto spec = OperationCommands::flatpakMutation(m_flatpakScope, action, m_selectedRemote, targets);
    startProcess(spec.program, spec.arguments, ProcessPurpose::FlatpakMutation, tr("Applying Flatpak changes"));
}

void AppController::startProcess(const QString& program, const QStringList& arguments, ProcessPurpose purpose, const QString& title) {
    if (m_runner.running()) {
        setStatus(tr("Another operation is already running"));
        return;
    }
    m_processPurpose = purpose;
    setBusy(true, title);
    if (!m_runner.start(program, arguments)) {
        m_processPurpose = ProcessPurpose::None;
        setBusy(false);
        setStatus(tr("Unable to start %1").arg(program));
    }
}

QStringList AppController::scopedFlatpakArgs() const {
    return {m_flatpakScope == QStringLiteral("user") ? QStringLiteral("--user") : QStringLiteral("--system")};
}

void AppController::loadFlatpaks() {
    m_flatpakAvailable = !QStandardPaths::findExecutable(QStringLiteral("flatpak")).isEmpty();
    emit flatpakStateChanged();
    if (!m_flatpakAvailable) {
        m_flatpakLoading = false;
        setStatus(tr("Flatpak is not installed"));
        return;
    }
    if (!m_flatpakWarningShown && !m_disableWarning) {
        m_flatpakWarningShown = true;
        m_flatpakWarningVisible = true;
        emit flatpakWarningChanged();
    }
    if (m_operationRunning) {
        return;
    }
    m_flatpakLoading = true;
    m_flatpakModel.clearSelection();
    emit flatpakStateChanged();
    startProcess(QStringLiteral("flatpak"), scopedFlatpakArgs() << QStringLiteral("remote-list") << QStringLiteral("--columns=name,url"), ProcessPurpose::FlatpakRemotes, tr("Loading Flatpak remotes"));
}

void AppController::installFlatpakSupport() {
    if (!m_snapshot.valid || m_operationRunning) {
        setStatus(tr("Native package metadata is still loading"));
        return;
    }
    beginNativePreview({QStringLiteral("flatpak")}, QStringLiteral("install"));
}

void AppController::startFlatpakListing() {
    if (m_selectedRemote.isEmpty() || m_remoteNames.isEmpty()) {
        m_flatpakModel.setItems({});
        m_flatpakLoading = false;
        setBusy(false);
        emit flatpakStateChanged();
        setStatus(m_remoteNames.isEmpty() ? tr("No Flatpak remotes configured") : tr("Choose a Flatpak remote"));
        return;
    }
    startProcess(QStringLiteral("flatpak"), scopedFlatpakArgs() << QStringLiteral("list") << QStringLiteral("--columns=ref"), ProcessPurpose::FlatpakInstalled, tr("Reading installed Flatpaks"));
}

void AppController::refreshFlatpakMetadata() {
    if (!m_flatpakAvailable || m_operationRunning) {
        return;
    }
    const auto spec = OperationCommands::flatpakAppstreamRefresh(m_flatpakScope);
    startProcess(spec.program, spec.arguments, ProcessPurpose::FlatpakAppstreamRefresh, tr("Refreshing Flatpak metadata"));
}

void AppController::updateFlatpaks() {
    if (!m_flatpakAvailable) {
        return;
    }
    m_pendingAction = QStringLiteral("flatpak-update");
    m_pendingSource = QStringLiteral("flatpak");
    m_pendingTargets.clear();
    m_transactionMap = {
        {QStringLiteral("action"), QStringLiteral("flatpak-update")},
        {QStringLiteral("success"), true},
        {QStringLiteral("summary"), tr("Update all installed applications and runtimes in the %1 scope.").arg(m_flatpakScope)},
        {QStringLiteral("additions"), QVariantList()},
        {QStringLiteral("removals"), QVariantList()},
        {QStringLiteral("conflicts"), QVariantList()},
    };
    m_transactionVisible = true;
    emit transactionChanged();
}

void AppController::updateSelectedFlatpaks() {
    if (!m_flatpakAvailable) {
        return;
    }
    const auto selected = m_flatpakModel.selectedItems();
    if (selected.isEmpty()) {
        setStatus(tr("Select at least one Flatpak update first"));
        return;
    }
    const auto refs = uniqueTargets(selected, true);
    m_pendingAction = QStringLiteral("flatpak-update-selected");
    m_pendingSource = QStringLiteral("flatpak");
    m_pendingTargets = refs;
    m_transactionMap = {
        {QStringLiteral("action"), QStringLiteral("flatpak-update-selected")},
        {QStringLiteral("success"), true},
        {QStringLiteral("summary"), tr("Update the selected Flatpak applications and runtimes in the %1 scope.").arg(m_flatpakScope)},
        {QStringLiteral("additions"), variantList(refs)},
        {QStringLiteral("removals"), QVariantList()},
        {QStringLiteral("conflicts"), QVariantList()},
    };
    m_transactionVisible = true;
    emit transactionChanged();
}

void AppController::removeUnusedFlatpaks() {
    if (!m_flatpakAvailable) {
        return;
    }
    m_pendingAction = QStringLiteral("flatpak-unused");
    m_pendingSource = QStringLiteral("flatpak");
    m_transactionMap = {
        {QStringLiteral("action"), QStringLiteral("flatpak-unused")},
        {QStringLiteral("success"), true},
        {QStringLiteral("summary"), tr("Remove unused runtimes from the %1 scope.").arg(m_flatpakScope)},
        {QStringLiteral("additions"), QVariantList()},
        {QStringLiteral("removals"), QVariantList()},
        {QStringLiteral("conflicts"), QVariantList()},
    };
    m_transactionVisible = true;
    emit transactionChanged();
}

void AppController::addRemote(const QString& name, const QString& url) {
    if (!validRemoteName(name) || !QUrl(url).isValid() || (QUrl(url).scheme() != QStringLiteral("https") && QUrl(url).scheme() != QStringLiteral("http"))) {
        setStatus(tr("Enter a valid remote name and HTTPS URL"));
        return;
    }
    m_pendingAction = QStringLiteral("remote-add");
    const auto spec = OperationCommands::flatpakRemoteAdd(m_flatpakScope, name, url);
    startProcess(spec.program, spec.arguments, ProcessPurpose::FlatpakMutation, tr("Adding Flatpak remote"));
}

void AppController::removeRemote(const QString& name) {
    if (name.isEmpty() || name == QStringLiteral("flathub")) {
        setStatus(tr("The Flathub remote is protected"));
        return;
    }
    m_pendingAction = QStringLiteral("remote-remove");
    const auto spec = OperationCommands::flatpakRemoteRemove(m_flatpakScope, name);
    startProcess(spec.program, spec.arguments, ProcessPurpose::FlatpakMutation, tr("Removing Flatpak remote"));
}

void AppController::installFlatpakref(const QString& location) {
    if (!validFlatpakrefLocation(location)) {
        setStatus(tr("Enter a Flatpakref URL or a readable local path"));
        return;
    }
    m_pendingAction = QStringLiteral("flatpakref");
    m_pendingSource = QStringLiteral("flatpak");
    m_pendingTargets = {location};
    m_transactionMap = {
        {QStringLiteral("action"), QStringLiteral("flatpakref")},
        {QStringLiteral("success"), true},
        {QStringLiteral("summary"), tr("Install this Flatpakref in the %1 scope:\n%2").arg(m_flatpakScope, location)},
        {QStringLiteral("additions"), variantList({location})},
        {QStringLiteral("removals"), QVariantList()},
        {QStringLiteral("conflicts"), QVariantList()},
    };
    m_transactionVisible = true;
    emit transactionChanged();
}

void AppController::onProcessOutput(const QString& text) {
    if (m_processPurpose == ProcessPurpose::FlatpakRemotes
        || m_processPurpose == ProcessPurpose::FlatpakInstalled
        || m_processPurpose == ProcessPurpose::FlatpakApps
        || m_processPurpose == ProcessPurpose::FlatpakRuntimes
        || m_processPurpose == ProcessPurpose::FlatpakUpdates
        || m_processPurpose == ProcessPurpose::OrphanQuery) {
        return;
    }
    m_consoleText += text;
    constexpr qsizetype maxConsoleCharacters = 2 * 1024 * 1024;
    if (m_consoleText.size() > maxConsoleCharacters) {
        m_consoleText.remove(0, m_consoleText.size() - maxConsoleCharacters);
        m_consoleText.prepend(tr("[Earlier output omitted]\n"));
    }
    qInfo().noquote() << text.trimmed();
    emit consoleChanged();
}

void AppController::onProcessError(const QString& text) {
    m_consoleText += text;
    qWarning().noquote() << text.trimmed();
    emit consoleChanged();
}

void AppController::onProcessFinished(int exitCode, QProcess::ExitStatus status, const QString& output, bool cancelled) {
    const bool success = !cancelled && status == QProcess::NormalExit && exitCode == 0;
    const auto purpose = m_processPurpose;
    m_processPurpose = ProcessPurpose::None;

    if (purpose == ProcessPurpose::FlatpakRemotes) {
        if (!success) {
            m_flatpakLoading = false;
            setBusy(false);
            emit flatpakStateChanged();
            setStatus(tr("Unable to read Flatpak remotes. Review the console output."));
            return;
        }
        m_remotes = FlatpakParser::parseRemotes(output);
        m_remoteNames.clear();
        for (const auto& remote : m_remotes) {
            m_remoteNames << remote.name;
        }
        if (!m_selectedRemote.isEmpty() && !m_remoteNames.contains(m_selectedRemote)) {
            m_selectedRemote.clear();
        }
        if (m_selectedRemote.isEmpty() && m_remoteNames.contains(QStringLiteral("flathub"))) {
            m_selectedRemote = QStringLiteral("flathub");
        } else if (m_selectedRemote.isEmpty() && !m_remoteNames.isEmpty()) {
            m_selectedRemote = m_remoteNames.first();
        }
        emit flatpakStateChanged();
        startFlatpakListing();
        return;
    }
    if (purpose == ProcessPurpose::FlatpakInstalled) {
        if (!success) {
            m_flatpakLoading = false;
            setBusy(false);
            emit flatpakStateChanged();
            setStatus(tr("Unable to read installed Flatpaks. Review the console output."));
            return;
        }
        m_installedFlatpakRefs = output.split('\n', Qt::SkipEmptyParts);
        startProcess(QStringLiteral("flatpak"), scopedFlatpakArgs() << QStringLiteral("remote-ls") << m_selectedRemote << QStringLiteral("--app") << QStringLiteral("--columns=ref,version,installed-size,name,description"), ProcessPurpose::FlatpakApps, tr("Loading Flatpak applications"));
        return;
    }
    if (purpose == ProcessPurpose::FlatpakApps) {
        if (!success) {
            m_flatpakLoading = false;
            setBusy(false);
            emit flatpakStateChanged();
            setStatus(tr("Unable to read the selected Flatpak remote."));
            return;
        }
        m_flatpakAppsOutput = output;
        startProcess(QStringLiteral("flatpak"), scopedFlatpakArgs() << QStringLiteral("remote-ls") << m_selectedRemote << QStringLiteral("--runtime") << QStringLiteral("--columns=ref,version,installed-size,name,description"), ProcessPurpose::FlatpakRuntimes, tr("Loading Flatpak runtimes"));
        return;
    }
    if (purpose == ProcessPurpose::FlatpakRuntimes) {
        if (!success) {
            m_flatpakLoading = false;
            setBusy(false);
            emit flatpakStateChanged();
            setStatus(tr("Unable to read Flatpak runtimes."));
            return;
        }
        auto items = FlatpakParser::parseRows(m_flatpakAppsOutput, false, m_installedFlatpakRefs, m_snapshot.metadata);
        items += FlatpakParser::parseRows(output, true, m_installedFlatpakRefs, m_snapshot.metadata);
        std::sort(items.begin(), items.end(), [](const auto& first, const auto& second) { return first.name.toCaseFolded() < second.name.toCaseFolded(); });
        m_flatpakItemsPending = std::move(items);
        startProcess(QStringLiteral("flatpak"), scopedFlatpakArgs() << QStringLiteral("remote-ls") << m_selectedRemote << QStringLiteral("--updates") << QStringLiteral("--columns=ref"), ProcessPurpose::FlatpakUpdates, tr("Checking Flatpak updates"));
        return;
    }
    if (purpose == ProcessPurpose::FlatpakUpdates) {
        QSet<QString> updates;
        if (success) {
            for (const auto& ref : output.split('\n', Qt::SkipEmptyParts)) {
                updates.insert(ref.trimmed());
            }
        }
        for (auto& item : m_flatpakItemsPending) {
            item.upgradable = item.installed && updates.contains(item.ref);
            if (item.upgradable) {
                item.status = QStringLiteral("upgradable");
            }
        }
        m_flatpakModel.setItems(std::move(m_flatpakItemsPending));
        m_flatpakLoading = false;
        setBusy(false);
        emit flatpakStateChanged();
        setStatus(tr("%1 Flatpak entries ready").arg(m_flatpakModel.totalCount()));
        return;
    }
    if (purpose == ProcessPurpose::OrphanQuery) {
        setBusy(false);
        if (cancelled) {
            setStatus(tr("Orphan scan cancelled"));
            return;
        }
        if (!success) {
            setStatus(tr("Unable to read orphan packages"));
            return;
        }
        const auto targets = output.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (targets.isEmpty()) {
            setStatus(tr("No orphan packages found"));
            return;
        }
        beginNativePreview(targets, QStringLiteral("remove-orphans"));
        return;
    }
    if (purpose == ProcessPurpose::FlatpakAppstreamRefresh) {
        if (cancelled) {
            finishOperation(false, tr("Flatpak metadata refresh cancelled"));
            return;
        }
        if (success) {
            setStatus(tr("Flatpak metadata refreshed"));
            if (m_ready && !m_loading) {
                refreshRepositories();
            }
            loadFlatpaks();
        } else {
            finishOperation(false, tr("Flatpak metadata refresh failed"));
        }
        return;
    }
    if (purpose == ProcessPurpose::FlatpakBootstrap) {
        if (!success || cancelled) {
            finishOperation(false, cancelled ? tr("Flatpak setup cancelled") : tr("Unable to configure the Flathub remote"));
            loadFlatpaks();
            return;
        }
        if (m_bootstrapStep == 0) {
            m_bootstrapStep = 1;
            const auto spec = OperationCommands::flatpakRemoteAdd(m_bootstrapScope, QStringLiteral("flathub-verified"), QStringLiteral("https://flathub.org/repo/flathub.flatpakrepo"), true);
            startProcess(spec.program, spec.arguments, ProcessPurpose::FlatpakBootstrap, tr("Adding the verified Flathub subset"));
        } else {
            finishOperation(true, tr("Flatpak support is ready"));
            loadFlatpaks();
        }
        return;
    }
    if (purpose == ProcessPurpose::NativeMutation || purpose == ProcessPurpose::FlatpakMutation) {
        const auto action = m_pendingAction;
        setBusy(false);
        if (cancelled) {
            finishOperation(false, tr("Operation cancelled. Review the console output."));
            return;
        }
        if (success) {
            m_popularModel.clearSelection();
            m_repositoryModel.clearSelection();
            m_flatpakModel.clearSelection();
            finishOperation(true, tr("Processing finished successfully"));
            if (purpose == ProcessPurpose::NativeMutation) {
                if (action == QStringLiteral("install") && m_pendingTargets.contains(QStringLiteral("flatpak"))) {
                    m_flatpakAvailable = !QStandardPaths::findExecutable(QStringLiteral("flatpak")).isEmpty();
                    emit flatpakStateChanged();
                    if (m_flatpakAvailable) {
                        m_bootstrapScope = QStringLiteral("system");
                        m_bootstrapStep = 0;
                        const auto spec = OperationCommands::flatpakRemoteAdd(QStringLiteral("system"), QStringLiteral("flathub"), QStringLiteral("https://flathub.org/repo/flathub.flatpakrepo"));
                        startProcess(spec.program, spec.arguments, ProcessPurpose::FlatpakBootstrap, tr("Adding the Flathub remote"));
                    } else {
                        refreshRepositories();
                    }
                } else {
                    refreshRepositories();
                }
            } else if (action == QStringLiteral("remote-add") || action == QStringLiteral("remote-remove") || action == QStringLiteral("flatpakref")) {
                loadFlatpaks();
            } else {
                loadFlatpaks();
            }
        } else {
            finishOperation(false, tr("The operation failed. Review the console output."));
        }
        return;
    }
    setBusy(false);
    if (!success) {
        setStatus(tr("Process failed"));
    }
}

void AppController::finishOperation(bool success, const QString& message) {
    setBusy(false);
    m_resultSuccess = success;
    m_resultMessage = message;
    m_resultVisible = true;
    emit resultChanged();
    setStatus(message);
    if (!success) {
        m_consoleText += '\n' + message + '\n';
        emit consoleChanged();
    }
}

void AppController::setStatus(const QString& message) {
    if (m_statusMessage == message) {
        return;
    }
    m_statusMessage = message;
    qInfo().noquote() << message;
    emit statusMessageChanged();
}

void AppController::setBusy(bool value, const QString& title) {
    if (!title.isEmpty()) {
        m_operationTitle = title;
    }
    if (m_operationRunning == value) {
        emit operationRunningChanged();
        return;
    }
    m_operationRunning = value;
    emit operationRunningChanged();
}

void AppController::setSystemReady(bool value) {
    if (m_systemReady == value) {
        return;
    }
    m_systemReady = value;
    emit systemReadyChanged();
}

void AppController::openHelp() {
    const QStringList candidates = {
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../share/doc/cachyos-catalog/cachyos-catalog.html")),
        QStringLiteral("/usr/share/doc/cachyos-catalog/cachyos-catalog.html"),
    };
    for (const auto& candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(candidate));
            return;
        }
    }
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/CachyOS/packageinstaller")));
}

bool AppController::validRemoteName(const QString& name) {
    return QRegularExpression(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$")).match(name).hasMatch();
}

bool AppController::validFlatpakrefLocation(const QString& location) {
    const QUrl url(location);
    if (url.isValid() && (url.scheme() == QStringLiteral("https") || url.scheme() == QStringLiteral("http"))) {
        return true;
    }
    const QFileInfo fileInfo(location);
    if (!fileInfo.exists() || !fileInfo.isFile() || !fileInfo.isReadable()) {
        return false;
    }
    QFile file(location);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const auto contents = file.read(64 * 1024);
    return location.endsWith(QStringLiteral(".flatpakref"), Qt::CaseInsensitive)
        || contents.contains("[Flatpak Ref]");
}

void AppController::retry() {
    if (!m_fatalError.isEmpty()) {
        refreshRepositories();
    } else if (m_page == QStringLiteral("flatpak")) {
        loadFlatpaks();
    } else {
        refreshRepositories();
    }
}
