#include "mainwindow.h"

#include "confirm_dialog.h"
#include "console_page.h"
#include "discover_page.h"
#include "flatpak_page.h"
#include "logging.h"
#include "navbar.h"
#include "pacman_cache.h"
#include "package_detail.h"
#include "progress_overlay.h"
#include "remotes_dialog.h"
#include "repo_page.h"
#include "settings_page.h"
#include "theme.h"

#include <QApplication>
#include <QCheckBox>
#include <QPointer>
#include <QDir>
#include <QFile>
#include <QFile>
#include <QHBoxLayout>
#include <QTextStream>
#include <QTimer>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QStackedWidget>
#include <QtConcurrent>

#include <QCloseEvent>

namespace {

/// Reference `isFilteredName`: library/developer/debug packages.
bool isFilteredName(const QString& name) {
    return ((name.startsWith(QLatin1String("lib")) && !name.startsWith(QLatin1String("libre")))
        || name.endsWith(QLatin1String("-dev")) || name.endsWith(QLatin1String("-dbg"))
        || name.endsWith(QLatin1String("-dbgsym")) || name.endsWith(QLatin1String("-debug"))
        || name.endsWith(QLatin1String("-devel")));
}

QString formatBytes(qint64 bytes) {
    if (bytes < 1024) {
        return QStringLiteral("%1 B").arg(bytes);
    }
    if (bytes < 1024 * 1024) {
        return QStringLiteral("%1 KB").arg(bytes / 1024);
    }
    if (bytes < 1024 * 1024 * 1024) {
        return QStringLiteral("%1 MB").arg(bytes / (1024 * 1024));
    }
    return QStringLiteral("%1 GB").arg(bytes / (1024 * 1024 * 1024));
}

}  // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle(tr("CachyOS Package Installer"));
    setWindowIcon(QIcon(QStringLiteral(":/icons/app-icon-256.png")));
    resize(1180, 760);
    setup();
}

MainWindow::~MainWindow() {
    m_settingsStore.setValue(QStringLiteral("geometry"), saveGeometry());
}

void MainWindow::setup() {
    auto* central = new QWidget(this);
    setCentralWidget(central);
    auto* rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_nav = new NavBar(central);
    m_stack = new QStackedWidget(central);

    m_discover = new DiscoverPage(m_stack);
    m_repoPage = new RepoPage(m_stack);
    m_flatpakPage = new FlatpakPage(m_stack);
    m_console = new ConsolePage(m_stack);
    m_settings = new SettingsPage(m_stack);

    m_stack->addWidget(m_discover);
    m_stack->addWidget(m_repoPage);
    m_stack->addWidget(m_flatpakPage);
    m_stack->addWidget(m_console);
    m_stack->addWidget(m_settings);

    m_nav->addItem(QStringLiteral("view-grid"), tr("Discover"));
    m_nav->addItem(QStringLiteral("package"), tr("Packages"));
    m_nav->addItem(QStringLiteral("application-x-addon"), tr("Flatpak"));
    m_nav->addItem(QStringLiteral("utilities-terminal"), tr("Console"));
    m_nav->addItem(QStringLiteral("settings"), tr("Settings"), /*bottom=*/true);

    rootLayout->addWidget(m_nav);
    rootLayout->addWidget(m_stack, 1);

    // navigation wiring
    connect(m_nav, &NavBar::itemActivated, this, &MainWindow::navigate);
    connect(m_stack, &QStackedWidget::currentChanged, this, [this](int idx) {
        if (m_nav) {
            m_nav->setCurrentIndex(idx);
        }
    });

    // page -> operations
    connect(m_discover, &DiscoverPage::installRequested, this, &MainWindow::confirmAndInstall);
    connect(m_discover, &DiscoverPage::uninstallRequested, this, &MainWindow::confirmAndUninstall);
    connect(m_repoPage, &RepoPage::installRequested, this, &MainWindow::confirmAndInstall);
    connect(m_repoPage, &RepoPage::uninstallRequested, this, &MainWindow::confirmAndUninstall);
    connect(m_repoPage, &RepoPage::upgradeAllRequested, this, &MainWindow::upgradeAll);
    connect(m_repoPage, &RepoPage::removeOrphansRequested, this, &MainWindow::removeOrphans);
    connect(m_repoPage, &RepoPage::infoRequested, this, &MainWindow::showPackageInfo);

    connect(m_flatpakPage, &FlatpakPage::installRequested, this, &MainWindow::confirmFlatpakInstall);
    connect(m_flatpakPage, &FlatpakPage::uninstallRequested, this, &MainWindow::confirmFlatpakUninstall);
    connect(m_flatpakPage, &FlatpakPage::updateAllRequested, this, &MainWindow::flatpakUpdateAll);
    connect(m_flatpakPage, &FlatpakPage::removeUnusedRequested, this, &MainWindow::flatpakRemoveUnused);
    connect(m_flatpakPage, &FlatpakPage::manageRemotesRequested, this, &MainWindow::manageRemotes);
    connect(m_flatpakPage, &FlatpakPage::scopeChanged, this, &MainWindow::onFlatpakScopeChanged);
    connect(m_flatpakPage, &FlatpakPage::remoteChanged, this, &MainWindow::onFlatpakRemoteChanged);
    connect(m_flatpakPage, &FlatpakPage::infoRequested, this, [this](const QString& appId) {
        QMessageBox::information(this, appId, appId);
    });

    connect(m_settings, &SettingsPage::themeChanged, this, [](const QString& mode) {
        QSettings().setValue(QStringLiteral("theme"), mode);
        theme::apply(theme::modeFromString(mode));
    });
    connect(m_settings, &SettingsPage::flatpakVisibilityChanged, this, [this](bool visible) {
        QSettings().setValue(QStringLiteral("showFlatpak"), visible);
        m_nav->setItemEnabled(2, visible);
        if (!visible && m_stack->currentIndex() == 2) {
            navigate(0);
        }
    });

    // console wiring
    connect(&m_cmd, &Cmd::outputAvailable, m_console, &ConsolePage::appendOutput);
    connect(&m_cmd, &Cmd::errorAvailable, m_console, &ConsolePage::appendError);
    connect(&m_cmd, &Cmd::finished, this, [this](bool ok, int) {
        logging::debug(QStringLiteral("Cmd finished: ok=%1 last=%2 pending=%3")
                           .arg(ok).arg(m_lastCommand).arg(m_pendingCommands.size()));
        m_console->setRunning(false);
        m_progress->hide();

        // chained flatpak uninstall loop
        if (!m_pendingCommands.isEmpty()) {
            if (ok) {
                runNextPendingCommand();
            } else {
                m_pendingCommands.clear();
                m_opInProgress = false;
                onOperationDone(false,
                    QString(), tr("Problem detected during last operation, please inspect the console output."),
                    false, true);
            }
            return;
        }

        // flatpak bootstrap: after installing the flatpak package, add flathub
        // remotes (reference behavior)
        if (m_bootstrapFlatpak) {
            m_bootstrapFlatpak = false;
            if (ok) {
                bootstrapFlathubRemotes();
                return;
            }
        }

        if (m_lastCommand == QStringLiteral("flathub bootstrap")) {
            m_opInProgress = false;
            refreshAll();
            if (ok) {
                m_flatpakLoaded = false;
                m_flatpakFirstList = true;
                QMessageBox::warning(this, tr("Needs re-login"),
                    tr("You might need to logout/login to see installed items in the menu"));
                m_stack->setCurrentIndex(2);
                m_nav->setCurrentIndex(2);
                loadFlatpakAsync(true);
            } else {
                QMessageBox::critical(this, tr("Flathub remote failed"),
                    tr("Flathub remote could not be added"));
            }
            return;
        }

        m_opInProgress = false;
        if (m_lastCommand == QStringLiteral("flatpak install")
            || m_lastCommand == QStringLiteral("flatpak install --from")
            || m_lastCommand == QStringLiteral("flatpak update")
            || m_lastCommand == QStringLiteral("flatpak uninstall --unused")) {
            onOperationDone(ok, tr("Processing finished successfully."),
                tr("Problem detected during last operation, please inspect the console output."),
                false, true);
        } else {
            onOperationDone(ok, tr("Processing finished successfully."),
                tr("Problem detected while installing, please inspect the console output."),
                true, false);
        }
    });
    connect(m_console, &ConsolePage::inputSubmitted, this, [this](const QByteArray& data) {
        m_cmd.write(data);
    });
    connect(m_console, &ConsolePage::cancelRequested, this, [this] {
        if (m_cmd.isRunning()) {
            m_cmd.terminate();
        }
    });

    // progress overlay
    m_progress = new ProgressOverlay(tr("Please wait..."), this);
    connect(m_progress, &ProgressOverlay::cancelRequested, this, [this] {
        if (m_cmd.isRunning()) {
            m_cmd.terminate();
        }
    });

    // initial page
    m_stack->setCurrentIndex(0);
    m_nav->setCurrentIndex(0);

    // settings
    m_settings->setAppVersion(QStringLiteral("1.0.0"));
    m_settings->loadSettings();

    // showFlatpak preference (reference parity: tab hidden when disabled)
    const bool showFlatpak = m_settingsStore.value(QStringLiteral("showFlatpak"), true).toBool();
    m_nav->setItemEnabled(2, showFlatpak);

    m_flatpakPage->setBackend(&m_flatpak);
    m_flatpak.setUserScope(false);

    // data
    auto alpmManager = alpm::AlpmManager::init();
    if (!alpmManager) {
        QMessageBox::critical(this, tr("CachyOS Package Installer"), tr("Cannot initialize ALPM library"));
        return;
    }
    m_alpm = std::make_shared<alpm::AlpmManager>(std::move(*alpmManager));

    loadAppStream();
    loadPopularAsync();
    loadPackageDataAsync();
}

void MainWindow::navigate(int index) {
    m_stack->setCurrentIndex(index);
    switch (index) {
    case 0:
        if (!m_popularLoaded) {
            loadPopularAsync();
        }
        break;
    case 1:
        if (!m_repoLoaded) {
            loadPackageDataAsync();
        } else {
            // refresh orphan visibility
            QPointer<QWidget> page = m_repoPage;
            QThreadPool::globalInstance()->start([page] {
                const QString out = runShellCommand(QStringLiteral("pacman -Qtdq"));
                const bool orphan = !out.trimmed().isEmpty();
                QMetaObject::invokeMethod(page, [page, orphan] { if (page) page->setProperty("orphans", orphan); }, Qt::QueuedConnection);
            });
        }
        break;
    case 2:
        if (!m_flatpakInstalled()) {
            ensureFlatpakAvailable();
        } else if (!m_flatpakLoaded) {
            showFlatpakWarning();
            loadFlatpakAsync(false);
        }
        break;
    case 4:
        m_settings->loadSettings();
        break;
    default:
        break;
    }
}

void MainWindow::loadAppStream() {
    m_discover->setAppStream(&m_appstream);
    m_appstream.loadAsync();
}

void MainWindow::loadPopularAsync() {
    if (m_popularLoaded) {
        return;
    }
    m_popularLoaded = true;

    QPointer<MainWindow> guard(this);
    QThreadPool::globalInstance()->start([guard] {
        // offline-first: cached copy, then bundled resource (network refresh
        // happens on the main thread below)
        if (!guard) return;
        QList<PopularEntry> entries;
        QString yaml;
        const QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        QFile cached(configDir + QStringLiteral("/pkglist.yaml"));
        if (cached.open(QIODevice::ReadOnly | QIODevice::Text)) {
            yaml = QString::fromUtf8(cached.readAll());
        }
        if (yaml.isEmpty()) {
            QFile bundled(QStringLiteral(":/pkglist.yaml"));
            if (bundled.open(QIODevice::ReadOnly | QIODevice::Text)) {
                yaml = QString::fromUtf8(bundled.readAll());
            }
        }
        if (!yaml.isEmpty()) {
            PkgList::parse(yaml, entries);
        }
        QMetaObject::invokeMethod(guard, [guard, entries] {
            if (!guard) return;
            guard->m_popularEntries = entries;
            guard->buildPopular();
        }, Qt::QueuedConnection);
    });

    // async network refresh of the curated list (reference fetch_net_pkglist)
    auto* manager = new QNetworkAccessManager(this);
    connect(manager, &QNetworkAccessManager::finished, this, [this, manager](QNetworkReply* reply) {
        reply->deleteLater();
        manager->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            const QByteArray data = reply->readAll();
            const QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
            QDir().mkpath(configDir);
            QFile f(configDir + QStringLiteral("/pkglist.yaml"));
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                f.write(data);
            }
            QList<PopularEntry> entries;
            PkgList::parse(QString::fromUtf8(data), entries);
            if (!entries.isEmpty()) {
                m_popularEntries = entries;
                buildPopular();
            }
        }
    });
    QNetworkRequest req{QUrl(QStringLiteral("https://raw.githubusercontent.com/cachyos/packageinstaller/develop/pkglist.yaml"))};
    req.setTransferTimeout(100000);
    manager->get(req);
}

void MainWindow::buildPopular() {
    QHash<QString, QString> versions;
    QHash<QString, QString> descriptions;
    for (auto it = m_candidates.constBegin(); it != m_candidates.constEnd(); ++it) {
        versions.insert(it.key(), it.value().value(0));
        descriptions.insert(it.key(), it.value().value(1));
    }
    m_discover->setEntries(m_popularEntries, m_installedNames, QSet<QString>(m_upgradable.begin(), m_upgradable.end()),
        versions, descriptions);
}

void MainWindow::loadPackageDataAsync() {
    if (m_repoLoaded) {
        return;
    }
    m_progress->setStatus(tr("Fetching information about packages..."));
    m_progress->show();

    QPointer<MainWindow> guard(this);
    QThreadPool::globalInstance()->start([guard, this] {
        if (!guard) return;
        PacmanCache cache(m_alpm);
        const QMap<QString, QStringList> candidates = cache.candidates();
        const QStringList upgradable = cache.upgradeCandidates();
        const QMap<QString, QString> installedVersions = m_alpm->listInstalledVersions();
        QStringList installedNames;
        installedNames.reserve(installedVersions.size());
        for (auto it = installedVersions.constBegin(); it != installedVersions.constEnd(); ++it) {
            installedNames << it.key();
        }
        QMetaObject::invokeMethod(guard, [guard, candidates, upgradable, installedVersions, installedNames] {
            if (!guard) return;
            guard->m_candidates = candidates;
            guard->m_upgradable = upgradable;
            guard->m_installedVersions = installedVersions;
            guard->m_installedNames = QSet<QString>(installedNames.begin(), installedNames.end());
            guard->m_repoLoaded = true;
            guard->m_progress->hide();

            // repo table rows
            std::vector<PackageRow> rows;
            rows.reserve(candidates.size());
            for (auto it = candidates.constBegin(); it != candidates.constEnd(); ++it) {
                PackageRow row;
                row.name = it.key();
                row.version = it.value().value(0);
                row.description = it.value().value(1);
                const bool installed = guard->m_installedNames.contains(it.key());
                const bool upgradableFlag = guard->m_upgradable.contains(it.key());
                row.state = !installed ? PackageRow::State::NotInstalled
                    : (upgradableFlag ? PackageRow::State::Upgradable : PackageRow::State::Installed);
                row.hiddenByLibFilter = isFilteredName(it.key());
                rows.push_back(std::move(row));
            }
            guard->m_repoPage->setPackageRows(std::move(rows));

            // orphan visibility
            QPointer<QWidget> page = guard->m_repoPage;
            QThreadPool::globalInstance()->start([page, guard] {
                if (!guard) return;
                const QString out = runShellCommand(QStringLiteral("pacman -Qtdq"));
                const bool orphan = !out.trimmed().isEmpty();
                QMetaObject::invokeMethod(page, [page, orphan] { if (page) page->setProperty("orphans", orphan); }, Qt::QueuedConnection);
            });

            if (!guard->m_popularEntries.isEmpty()) {
                guard->buildPopular();
            }

            if (qEnvironmentVariableIsSet("CACHYOS_PI_SELFTEST")) {
                // automated test hook: dump state and exit
                QFile out(QString::fromLocal8Bit(qgetenv("CACHYOS_PI_SELFTEST")));
                if (out.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    QTextStream ts(&out);
                    ts << "rows=" << guard->m_repoPage->packageCount()
                       << " upgradable=" << guard->m_upgradable.size()
                       << " installed=" << guard->m_installedNames.size()
                       << " popular=" << guard->m_popularEntries.size()
                       << "\n";
                    out.flush();
                }
                QTimer::singleShot(100, qApp, &QCoreApplication::quit);
            }
        }, Qt::QueuedConnection);
    });
}

void MainWindow::refreshAll() {
    m_repoLoaded = false;
    m_popularLoaded = false;
    loadPopularAsync();
    loadPackageDataAsync();
}

bool MainWindow::m_flatpakInstalled() const {
    return m_installedNames.contains(QStringLiteral("flatpak"));
}

void MainWindow::ensureFlatpakAvailable() {
    const auto answer = QMessageBox::question(this, tr("Flatpak not installed"),
        tr("Flatpak is not currently installed.\nOK to go ahead and install it?"));
    if (answer != QMessageBox::Yes) {
        navigate(0);
        return;
    }
    m_bootstrapFlatpak = true;
    confirmAndInstall({QStringLiteral("flatpak")});
}

void MainWindow::bootstrapFlathubRemotes() {
    switchConsole(tr("Adding Flathub remotes..."));
    m_progress->setStatus(tr("Adding Flathub remotes..."));
    m_progress->show();
    m_progress->setBusy(true);
    m_opInProgress = true;
    m_lastCommand = QStringLiteral("flathub bootstrap");
    m_console->setRunning(true);
    m_console->clear();
    const QString cmd = QStringLiteral(
        "pkexec /bin/bash -c \"flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo "
        "&& flatpak remote-add --if-not-exists --subset=verified flathub-verified https://flathub.org/repo/flathub.flatpakrepo\"");
    if (!m_cmd.run(cmd)) {
        m_opInProgress = false;
        m_progress->hide();
        QMessageBox::critical(this, tr("Flathub remote failed"), tr("Flathub remote could not be added"));
        return;
    }
    // handled by the Cmd::finished handler which then shows the re-login hint
}

void MainWindow::runPacmanOperation(const QStringList& names, const QString& title, bool remove,
    const QString& command) {
    switchConsole(title);
    m_progress->setStatus(title);
    m_progress->show();
    m_progress->setBusy(true);
    m_opInProgress = true;
    m_lastCommand = command;
    m_console->setRunning(true);
    m_console->clear();
    if (!m_cmd.run(command)) {
        m_opInProgress = false;
        m_progress->hide();
        QMessageBox::critical(this, tr("Error"), tr("Could not start the requested operation."));
    }
}

void MainWindow::confirmAndInstall(const QStringList& names) {
    if (m_opInProgress || names.isEmpty()) {
        return;
    }
    if (!m_alpm) {
        QMessageBox::critical(this, tr("Error"), tr("Cannot initialize ALPM library"));
        return;
    }

    const QString namesText = names.join(QLatin1Char(' '));
    QString statusText;
    const auto preview = m_alpm->displayInstallTargets(names, true);
    statusText = preview.statusText;

    QString conflictMsg;
    m_alpm->prepareAddTrans(names, conflictMsg);
    const bool hasConflicts = !conflictMsg.trimmed().isEmpty();

    if (hasConflicts) {
        QMessageBox box(this);
        box.setText(QStringLiteral("<b>%1</b>").arg(tr("The following packages have conflicts.")));
        box.setInformativeText(namesText + QStringLiteral("\n\n") + conflictMsg.trimmed());
        auto* replaceBtn = box.addButton(tr("Replace"), QMessageBox::AcceptRole);
        auto* ignoreBtn = box.addButton(tr("Ignore"), QMessageBox::RejectRole);
        box.setDefaultButton(ignoreBtn);
        box.exec();
        if (qobject_cast<QPushButton*>(box.clickedButton()) != replaceBtn) {
            return;
        }
    }

    ConfirmDialog dialog(ConfirmDialog::Mode::Install, namesText, statusText, preview.details,
        hasConflicts, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    runPacmanOperation(names, tr("Installing packages..."), false,
        QStringLiteral("pkexec pacman -S ") + namesText);
}

void MainWindow::confirmAndUninstall(const QStringList& names) {
    if (m_opInProgress || names.isEmpty()) {
        return;
    }
    if (!m_alpm) {
        QMessageBox::critical(this, tr("Error"), tr("Cannot initialize ALPM library"));
        return;
    }

    const QString namesText = names.join(QLatin1Char(' '));
    const auto preview = m_alpm->displayRemoveTargets(names, true);

    ConfirmDialog dialog(ConfirmDialog::Mode::Remove, namesText, preview.statusText, preview.details,
        false, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    // reference behavior: removals always run with --noconfirm
    runPacmanOperation(names, tr("Uninstalling packages..."), true,
        QStringLiteral("pkexec pacman -R --noconfirm ") + namesText);
}

void MainWindow::upgradeAll() {
    if (m_opInProgress) {
        return;
    }
    runPacmanOperation({}, tr("Upgrading system..."), false, QStringLiteral("pkexec pacman -Syu"));
}

void MainWindow::removeOrphans() {
    if (m_opInProgress) {
        return;
    }
    const QString names = runShellCommand(QStringLiteral("pacman -Qdtq | tr '\\n' ' '")).trimmed();
    if (names.isEmpty()) {
        return;
    }
    QMessageBox::warning(this, tr("Warning"),
        tr("Potentially dangerous operation.\nPlease make sure you check carefully the list of packages to be removed."));
    const QString namesText = names;
    const auto preview = m_alpm->displayRemoveTargets(names.split(QLatin1Char(' '), Qt::SkipEmptyParts), true);
    ConfirmDialog dialog(ConfirmDialog::Mode::Remove, namesText, preview.statusText, preview.details, false, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    runPacmanOperation(names.split(QLatin1Char(' ')), tr("Uninstalling packages..."), true,
        QStringLiteral("pkexec pacman -R --noconfirm ") + namesText);
}

void MainWindow::showPackageInfo(const QString& name) {
    if (!m_alpm) {
        return;
    }
    const auto info = m_alpm->getPackageInfo(name);
    if (!info) {
        QMessageBox::information(this, name, tr("Package not found in any configured repository."));
        return;
    }
    QString text;
    text += QStringLiteral("<b>%1</b><p>%2<p>").arg(info->name, info->desc.toHtmlEscaped());
    text += tr("Version: %1<br>").arg(info->version);
    text += tr("Repository: %1<br>").arg(info->repo);
    text += tr("Architecture: %1<br>").arg(info->arch);
    text += tr("Download size: %1<br>").arg(formatBytes(info->downloadSize));
    text += tr("Installed size: %1<br>").arg(formatBytes(info->installedSize));
    if (!info->url.isEmpty()) {
        text += tr("URL: %1<br>").arg(info->url.toHtmlEscaped());
    }
    if (!info->packager.isEmpty()) {
        text += tr("Packager: %1<br>").arg(info->packager);
    }
    if (!info->installReason.isEmpty()) {
        text += tr("Install reason: %1<br>").arg(info->installReason);
    }
    if (!info->licenses.isEmpty()) {
        text += tr("Licenses: %1<br>").arg(info->licenses.join(QLatin1String(", ")));
    }
    if (!info->groups.isEmpty()) {
        text += tr("Groups: %1<br>").arg(info->groups.join(QLatin1String(", ")));
    }
    if (!info->depends.isEmpty()) {
        text += tr("Depends on: %1<br>").arg(info->depends.join(QLatin1String(", ")));
    }
    if (!info->optDepends.isEmpty()) {
        text += tr("Optional dependencies: %1<br>").arg(info->optDepends.join(QLatin1String(", ")));
    }
    QMessageBox box(this);
    box.setWindowTitle(tr("Package info"));
    box.setTextFormat(Qt::RichText);
    box.setText(text);
    box.setStandardButtons(QMessageBox::Close);
    box.exec();
}

void MainWindow::showFlatpakWarning() {
    if (m_flatpakPage->warningShown() || m_settingsStore.value(QStringLiteral("disableWarning"), false).toBool()) {
        return;
    }
    QMessageBox box(QMessageBox::Warning, tr("Warning"),
        tr("CachyOS includes this repository of flatpaks for the users' convenience only, and "
           "is not responsible for the functionality of the individual flatpaks themselves. "
           "For more, consult flatpaks in the Wiki."),
        QMessageBox::Close, this);
    auto* cb = new QCheckBox(tr("Do not show this message again"), &box);
    box.setCheckBox(cb);
    connect(cb, &QCheckBox::toggled, this, [this](bool on) {
        m_settingsStore.setValue(QStringLiteral("disableWarning"), on);
    });
    box.exec();
    m_flatpakPage->markWarningShown();
}

void MainWindow::loadFlatpakAsync(bool force) {
    if (m_opInProgress) {
        return;
    }
    m_flatpakPage->setBusy(true);
    m_progress->setStatus(tr("Fetching Flatpak applications..."));
    m_progress->show();

    const QString remote = m_flatpakPage->currentRemote();
    const bool userScope = m_flatpakPage->userScope();
    m_flatpak.setUserScope(userScope);

    QPointer<MainWindow> guard(this);
    QThreadPool::globalInstance()->start([guard, force, remote, userScope, this] {
        if (!guard) return;
        // first listing per session refreshes remote appstream (reference parity)
        if (m_flatpakFirstList) {
            m_flatpak.updateAppstream();
        }
        const QStringList remotes = m_flatpak.listRemotes();
        const QStringList installedApps = m_flatpak.listInstalledRefs(false);
        const QStringList installedRuntimes = m_flatpak.listInstalledRefs(true);

        const QString targetRemote = remote.isEmpty() && !remotes.isEmpty() ? remotes.first() : remote;
        QList<FlatpakBackend::Ref> apps = m_flatpak.listAvailable(targetRemote, false);
        QList<FlatpakBackend::Ref> runtimes = m_flatpak.listAvailable(targetRemote, true);

        const QSet<QString> installedSet(installedApps.begin(), installedApps.end());
        const QSet<QString> installedRuntimesSet(installedRuntimes.begin(), installedRuntimes.end());
        for (auto& ref : apps) {
            ref.installed = installedSet.contains(ref.normalizedRef) || installedSet.contains(ref.appId);
        }
        for (auto& ref : runtimes) {
            ref.installed = installedRuntimesSet.contains(ref.normalizedRef);
        }
        const QString totalSize = m_flatpak.listInstalledSize();
        const int totalInstalled = installedApps.size() + installedRuntimes.size();

        QMetaObject::invokeMethod(guard, [guard, apps, runtimes, remotes, targetRemote, totalSize, totalInstalled] {
            if (!guard) return;
            QList<FlatpakBackend::Ref> all = apps + runtimes;
            guard->m_flatpakPage->setRemotes(remotes);
            guard->m_flatpakPage->setRefs(all, targetRemote);
            guard->m_flatpakPage->setCounts(all.size(), totalInstalled, totalSize);
            guard->m_flatpakPage->setBusy(false);
            guard->m_flatpakLoaded = true;
            guard->m_flatpakFirstList = false;
            guard->m_progress->hide();
        }, Qt::QueuedConnection);
    });
}

void MainWindow::onFlatpakScopeChanged(bool userScope) {
    if (m_opInProgress) {
        return;
    }
    m_progress->setStatus(tr("Switching Flatpak scope..."));
    m_progress->show();
    QPointer<MainWindow> guard(this);
    QThreadPool::globalInstance()->start([guard, userScope, this] {
        if (!guard) return;
        if (userScope) {
            guard->m_flatpak.ensureFlathubRemotes();
            guard->m_flatpak.updateAppstream();
        }
        QMetaObject::invokeMethod(guard, [guard] {
            if (!guard) return;
            guard->m_progress->hide();
            guard->m_flatpakLoaded = false;
            guard->loadFlatpakAsync(true);
        }, Qt::QueuedConnection);
    });
}

void MainWindow::onFlatpakRemoteChanged(const QString&) {
    if (!m_flatpakLoaded || m_opInProgress) {
        return;
    }
    loadFlatpakAsync(true);
}

void MainWindow::confirmFlatpakInstall(const QStringList& appIds) {
    if (m_opInProgress || appIds.isEmpty()) {
        return;
    }
    const QString namesText = appIds.join(QLatin1Char('\n'));
    ConfirmDialog dialog(ConfirmDialog::Mode::Install, namesText, QString(),
        namesText, false, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const QString cmd = m_flatpak.installCommand(m_flatpakPage->currentRemote(), appIds);
    switchConsole(tr("Installing Flatpak applications..."));
    m_progress->show();
    m_progress->setBusy(true);
    m_opInProgress = true;
    m_lastCommand = QStringLiteral("flatpak install");
    m_console->setRunning(true);
    m_console->clear();
    if (!m_cmd.run(cmd)) {
        m_opInProgress = false;
        m_progress->hide();
        QMessageBox::critical(this, tr("Error"), tr("Could not start the requested operation."));
    }
}

void MainWindow::confirmFlatpakUninstall(const QStringList& appIds) {
    if (m_opInProgress || appIds.isEmpty()) {
        return;
    }
    const QString namesText = appIds.join(QLatin1Char('\n'));
    ConfirmDialog dialog(ConfirmDialog::Mode::Remove, namesText, QString(), namesText, false, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    switchConsole(tr("Uninstalling Flatpak applications..."));
    m_progress->show();
    m_progress->setBusy(true);
    m_opInProgress = true;
    m_lastCommand = QStringLiteral("flatpak uninstall");
    m_console->setRunning(true);
    m_console->clear();

    // run uninstall per app (reference behavior)
    QStringList commands;
    for (const QString& app : appIds) {
        commands << m_flatpak.uninstallCommand(app);
    }
    m_pendingCommands = commands;
    runNextPendingCommand();
}

void MainWindow::runNextPendingCommand() {
    if (m_pendingCommands.isEmpty()) {
        // all done
        m_opInProgress = false;
        m_progress->hide();
        onOperationDone(true, tr("Processing finished successfully."),
            tr("Problem detected during last operation, please inspect the console output."),
            false, true);
        return;
    }
    const QString cmd = m_pendingCommands.takeFirst();
    if (!m_cmd.run(cmd)) {
        m_pendingCommands.clear();
        m_opInProgress = false;
        m_progress->hide();
        QMessageBox::critical(this, tr("Error"), tr("Could not start the requested operation."));
    }
}

void MainWindow::flatpakUpdateAll() {
    if (m_opInProgress) {
        return;
    }
    const QString cmd = m_flatpak.updateAllCommand();
    switchConsole(tr("Updating Flatpak applications..."));
    m_progress->show();
    m_progress->setBusy(true);
    m_opInProgress = true;
    m_lastCommand = QStringLiteral("flatpak update");
    m_console->setRunning(true);
    m_console->clear();
    if (!m_cmd.run(cmd)) {
        m_opInProgress = false;
        m_progress->hide();
        QMessageBox::critical(this, tr("Error"), tr("Could not start the requested operation."));
    }
}

void MainWindow::flatpakRemoveUnused() {
    if (m_opInProgress) {
        return;
    }
    const QString cmd = m_flatpak.removeUnusedCommand();
    switchConsole(tr("Removing unused Flatpak packages..."));
    m_progress->show();
    m_progress->setBusy(true);
    m_opInProgress = true;
    m_lastCommand = QStringLiteral("flatpak uninstall --unused");
    m_console->setRunning(true);
    m_console->clear();
    if (!m_cmd.run(cmd)) {
        m_opInProgress = false;
        m_progress->hide();
        QMessageBox::critical(this, tr("Error"), tr("Could not start the requested operation."));
    }
}

void MainWindow::manageRemotes() {
    RemotesDialog dialog(&m_flatpak, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    if (!dialog.installRef().isEmpty()) {
        installFlatpakRef(dialog.installRef(), dialog.userScope());
        return;
    }
    if (dialog.isChanged()) {
        m_flatpakLoaded = false;
        loadFlatpakAsync(true);
    }
}

void MainWindow::installFlatpakRef(const QString& ref, bool userScope) {
    if (m_opInProgress || ref.isEmpty()) {
        return;
    }
    m_flatpak.setUserScope(userScope);
    const QString cmd = m_flatpak.installRefCommand(ref);
    switchConsole(tr("Installing from Flatpakref..."));
    m_progress->show();
    m_progress->setBusy(true);
    m_opInProgress = true;
    m_lastCommand = QStringLiteral("flatpak install --from");
    m_console->setRunning(true);
    m_console->clear();
    if (!m_cmd.run(cmd)) {
        m_opInProgress = false;
        m_progress->hide();
        QMessageBox::critical(this, tr("Error"), tr("Could not start the requested operation."));
    }
}

void MainWindow::switchConsole(const QString& title) {
    m_console->setTitle(title);
    m_stack->setCurrentIndex(3);
    m_nav->setCurrentIndex(3);
    m_console->clear();
}

void MainWindow::onOperationDone(bool ok, const QString& successMessage, const QString& failureMessage,
    bool refreshRepo, bool refreshFlatpak) {
    m_console->setRunning(false);
    if (refreshFlatpak) {
        m_flatpakLoaded = false;
        loadFlatpakAsync(true);
    }
    if (refreshRepo) {
        refreshAll();
    }
    if (ok) {
        QMessageBox::information(this, tr("Done"), successMessage);
    } else {
        QMessageBox::critical(this, tr("Error"), failureMessage);
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (m_opInProgress) {
        const auto answer = QMessageBox::warning(this, tr("Quit?"),
            tr("Process still running, quitting might leave the system in an unstable state.<p><b>Are you sure you want to exit CachyOS Package Installer?</b></p>"),
            QMessageBox::Yes, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            event->ignore();
            return;
        }
    }
    m_cmd.halt();
    m_settingsStore.setValue(QStringLiteral("geometry"), saveGeometry());
    event->accept();
}
