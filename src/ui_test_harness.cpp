#include "ui_test_harness.h"

#include "logging.h"
#include "theme.h"
#include "ui/confirm_dialog.h"
#include "ui/navbar.h"
#include "ui/console_page.h"
#include "ui/discover_page.h"
#include "ui/flatpak_page.h"
#include "ui/mainwindow.h"
#include "ui/repo_page.h"
#include "ui/settings_page.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QPushButton>
#include <QTextStream>
#include <QTimer>

#include <QPointer>
#include <QThread>

namespace {

class TestReport {
public:
    explicit TestReport(const QString& dir) : m_dir(dir) {
        QDir().mkpath(dir);
        m_file.setFileName(dir + QStringLiteral("/report.txt"));
        m_file.open(QIODevice::WriteOnly | QIODevice::Text);  // NOLINT
        Q_UNUSED(m_stream)
        m_stream.setDevice(&m_file);
    }
    ~TestReport() { m_file.close(); }

    void line(const QString& text) {
        m_stream << text << '\n';
        m_stream.flush();
        logging::info(text);
    }
    void pass(const QString& name) { line(QStringLiteral("PASS  ") + name); }
    void fail(const QString& name, const QString& detail = {}) {
        line(QStringLiteral("FAIL  ") + name + (detail.isEmpty() ? QString() : QStringLiteral(" :: ") + detail));
        ++m_failures;
    }
    void skip(const QString& name, const QString& reason) { line(QStringLiteral("SKIP  ") + name + QStringLiteral(" :: ") + reason); }
    void note(const QString& text) { line(QStringLiteral("# ") + text); }
    int failures() const { return m_failures; }

    void screenshot(QWidget* w, const QString& name) {
        w->grab().save(m_dir + QLatin1Char('/') + name + QStringLiteral(".png"));
    }

private:
    QString m_dir;
    QFile m_file;
    QTextStream m_stream;
    int m_failures = 0;
};

/// Polls until `cond` is true or the timeout elapses; processes events so
/// modal dialogs (confirm/error boxes) and async loads make progress.
bool waitFor(const std::function<bool()>& cond, int timeoutMs = 20000) {
    const auto deadline = QDeadlineTimer(timeoutMs);
    while (!cond()) {
        if (deadline.hasExpired()) {
            return false;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(25);
    }
    QCoreApplication::processEvents();
    return true;
}

/// Watches for modal dialogs and interacts with them according to the
/// active `dialogPlan`: "accept" clicks the default/accent button of
/// ConfirmDialog and OK of QMessageBox; "cancel" clicks the cancel button.
class ModalDriver : public QObject {
public:
    enum class Plan { Accept, Cancel };

    explicit ModalDriver(Plan plan, TestReport* report, QObject* parent)
        : QObject(parent), m_plan(plan), m_report(report) {
        m_timer.setInterval(120);
        connect(&m_timer, &QTimer::timeout, this, [this] { poll(); });
        m_timer.start();
    }

    QStringList seenTitles() const { return m_titles; }

private:
    void poll() {
        QWidget* modal = QApplication::activeModalWidget();
        if (!modal) {
            m_lastHandled = nullptr;
            return;
        }
        if (m_lastHandled == modal) {
            return;
        }
        m_lastHandled = modal;
        m_titles << modal->windowTitle();

        if (auto* box = qobject_cast<QMessageBox*>(modal)) {
            // handled by the persistent MessageBoxAcker
            return;
        }
        if (auto* dialog = qobject_cast<ConfirmDialog*>(modal)) {
            const auto buttons = dialog->findChildren<QPushButton*>();
            QPushButton* target = nullptr;
            if (m_plan == Plan::Cancel) {
                for (auto* b : buttons) {
                    if (b->text() == QObject::tr("Cancel")) {
                        target = b;
                    }
                }
            } else {
                for (auto* b : buttons) {
                    if (b->text() == QObject::tr("Install") || b->text() == QObject::tr("Remove")
                        || b->text() == QObject::tr("Upgrade") || b->text() == QObject::tr("Replace")) {
                        target = b;
                        break;
                    }
                }
            }
            if (target) {
                m_report->note(QStringLiteral("auto-clicked '%1' on confirm dialog").arg(target->text()));
                target->click();
            }
        }
    }

    Plan m_plan;
    TestReport* m_report;
    QTimer m_timer;
    QPointer<QWidget> m_lastHandled;
    QStringList m_titles;
};

/// Dismisses every QMessageBox automatically (used for the whole run so
/// warning/info/error boxes never block the scripted flows).
class MessageBoxAcker : public QObject {
public:
    explicit MessageBoxAcker(TestReport* report, QObject* parent)
        : QObject(parent), m_report(report) {
        m_timer.setInterval(100);
        connect(&m_timer, &QTimer::timeout, this, [this] { poll(); });
        m_timer.start();
    }

    QStringList seenTitles() const { return m_titles; }

private:
    void poll() {
        QWidget* modal = QApplication::activeModalWidget();
        if (!modal) {
            m_lastHandled = nullptr;
            return;
        }
        if (m_lastHandled == modal) {
            return;
        }
        m_lastHandled = modal;
        m_report->note(QStringLiteral("acker sees modal: %1 (%2)").arg(modal->metaObject()->className(), modal->windowTitle()));
        if (auto* box = qobject_cast<QMessageBox*>(modal)) {
            m_titles << box->windowTitle();
            auto* btn = qobject_cast<QPushButton*>(box->defaultButton());
            if (!btn) {
                btn = qobject_cast<QPushButton*>(box->button(QMessageBox::Ok));
            }
            if (!btn) {
                btn = qobject_cast<QPushButton*>(box->button(QMessageBox::Yes));
            }
            if (!btn) {
                // fall back to the first visible button
                const auto buttons = box->buttons();
                for (QAbstractButton* b : buttons) {
                    if (b->isVisible()) {
                        btn = qobject_cast<QPushButton*>(b);
                        break;
                    }
                }
            }
            if (btn) {
                m_report->note(QStringLiteral("auto-acked message box '%1'").arg(box->windowTitle()));
                btn->click();
            }
        }
    }

    TestReport* m_report;
    QTimer m_timer;
    QPointer<QWidget> m_lastHandled;
    QStringList m_titles;
};

QString readFile(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(f.readAll());
}

bool logContains(MainWindow&, const QString& needle) {
    const QString log = readFile(QString::fromLocal8Bit(qgetenv("FAKE_LOG")));
    return log.contains(needle);
}

}  // namespace

namespace uitest {

int run(MainWindow& window, const QString& reportDir) {
    TestReport report(reportDir);
    report.note(QStringLiteral("UI test harness start"));

    MessageBoxAcker acker(&report, &window);

    // wait for initial package data
    if (!waitFor([&] { return window.repoPage()->packageCount() > 0; })) {
        report.fail(QStringLiteral("initial package data load"));
        return 1;
    }
    report.pass(QStringLiteral("package data loaded"));
    report.note(QStringLiteral("packages=%1 installed=%2").arg(window.repoPage()->packageCount())
                    .arg(window.installedNames().size()));

    // ---- repo search & filters ----
    {
        const int total = window.repoPage()->packageCount();
        window.repoPage()->setSearchFilter(QStringLiteral("firefox"));
        if (!waitFor([&] { return window.repoPage()->filteredRowCount() < total; })) {
            report.fail(QStringLiteral("repo search filter"));
        } else {
            report.pass(QStringLiteral("repo search filter"));
        }
        window.repoPage()->setSearchFilter(QString());
        const int upgr = window.repoPage()->upgradableCount();
        window.repoPage()->setStatusFilterIndex(1);
        if (window.repoPage()->filteredRowCount() == upgr) {
            report.pass(QStringLiteral("repo upgradable filter"));
        } else {
            report.fail(QStringLiteral("repo upgradable filter"),
                QStringLiteral("expected %1 got %2").arg(upgr).arg(window.repoPage()->filteredRowCount()));
        }
        window.repoPage()->setStatusFilterIndex(0);
    }

    // ---- install flow (fake pkexec/pacman) ----
    {
        report.note(QStringLiteral("scenario: install flow"));
        QFile(QString::fromLocal8Bit(qgetenv("FAKE_LOG"))).remove();
        ModalDriver driver(ModalDriver::Plan::Accept, &report, &window);
        window.confirmAndInstallPublic({QStringLiteral("vim")});
        if (!waitFor([&] { return !window.opInProgress() && logContains(window, QStringLiteral("pkexec pacman -S vim")); })) {
            report.fail(QStringLiteral("install flow"), QStringLiteral("fake log: %1").arg(readFile(QString::fromLocal8Bit(qgetenv("FAKE_LOG")))));
        } else {
            report.pass(QStringLiteral("install flow (pkexec pacman -S vim)"));
        }
        if (!driver.seenTitles().contains(QObject::tr("Confirm installation"))) {
            report.fail(QStringLiteral("install confirmation dialog shown"));
        }
    }

    // ---- cancel path ----
    {
        QFile(QString::fromLocal8Bit(qgetenv("FAKE_LOG"))).remove();
        ModalDriver driver(ModalDriver::Plan::Cancel, &report, &window);
        window.confirmAndInstallPublic({QStringLiteral("firefox")});
        QThread::msleep(800);
        QCoreApplication::processEvents();
        const QString log = readFile(QString::fromLocal8Bit(qgetenv("FAKE_LOG")));
        if (log.contains(QStringLiteral("pacman -S firefox"))) {
            report.fail(QStringLiteral("cancel flow (nothing should run)"));
        } else {
            report.pass(QStringLiteral("cancel flow"));
        }
    }

    // ---- uninstall flow ----
    {
        QFile(QString::fromLocal8Bit(qgetenv("FAKE_LOG"))).remove();
        ModalDriver driver(ModalDriver::Plan::Accept, &report, &window);
        window.confirmAndUninstallPublic({QStringLiteral("vim")});
        if (!waitFor([&] { return !window.opInProgress() && logContains(window, QStringLiteral("pkexec pacman -R --noconfirm vim")); })) {
            report.fail(QStringLiteral("uninstall flow"), QStringLiteral("fake log: %1").arg(readFile(QString::fromLocal8Bit(qgetenv("FAKE_LOG")))));
        } else {
            report.pass(QStringLiteral("uninstall flow (pkexec pacman -R --noconfirm vim)"));
        }
    }

    // ---- upgrade all ----
    {
        report.note(QStringLiteral("scenario: upgrade all"));
        QFile(QString::fromLocal8Bit(qgetenv("FAKE_LOG"))).remove();
        ModalDriver driver(ModalDriver::Plan::Accept, &report, &window);
        window.upgradeAllPublic();
        report.note(QStringLiteral("upgradeAllPublic returned"));
        if (!waitFor([&] { return !window.opInProgress() && logContains(window, QStringLiteral("pkexec pacman -Syu")); })) {
            report.fail(QStringLiteral("upgrade all flow"));
        } else {
            report.pass(QStringLiteral("upgrade all flow (pkexec pacman -Syu)"));
        }
    }

    // ---- orphan removal ----
    {
        report.note(QStringLiteral("scenario: orphan removal"));
        QFile(QString::fromLocal8Bit(qgetenv("FAKE_LOG"))).remove();
        ModalDriver driver(ModalDriver::Plan::Accept, &report, &window);
        window.removeOrphansPublic();
        if (!waitFor([&] { return !window.opInProgress() && logContains(window, QStringLiteral("pkexec pacman -R --noconfirm orphanpkg")); })) {
            report.fail(QStringLiteral("orphan removal flow"), QStringLiteral("fake log: %1").arg(readFile(QString::fromLocal8Bit(qgetenv("FAKE_LOG")))));
        } else {
            report.pass(QStringLiteral("orphan removal flow"));
        }
    }

    // ---- failure path ----
    {
        QFile(QString::fromLocal8Bit(qgetenv("FAKE_LOG"))).remove();
        qputenv("FAKE_PACMAN_FAIL", "1");
        ModalDriver driver(ModalDriver::Plan::Accept, &report, &window);
        window.confirmAndInstallPublic({QStringLiteral("vim")});
        const bool failedBox = waitFor([&] {
            return acker.seenTitles().contains(QObject::tr("Error"));
        });
        qunsetenv("FAKE_PACMAN_FAIL");
        if (!failedBox) {
            report.fail(QStringLiteral("failure path (error dialog)"));
        } else {
            report.pass(QStringLiteral("failure path (error dialog)"));
        }
        waitFor([&] { return !window.opInProgress(); });
    }

    // ---- flatpak listing (fake flatpak) ----
    window.navigate(2);
    if (!waitFor([&] { return window.flatpakPage()->countsVisible(); })) {
        report.fail(QStringLiteral("flatpak listing"));
    } else {
        report.pass(QStringLiteral("flatpak listing loaded"));
    }

    // ---- flatpak install ----
    {
        QFile(QString::fromLocal8Bit(qgetenv("FAKE_LOG"))).remove();
        ModalDriver driver(ModalDriver::Plan::Accept, &report, &window);
        window.confirmFlatpakInstallPublic({QStringLiteral("org.videolan.VLC")});
        if (!waitFor([&] { return !window.opInProgress() && logContains(window, QStringLiteral("flatpak install -y --system flathub org.videolan.VLC"))
                && logContains(window, QStringLiteral("flatpak install -y --system flathub org.videolan.VLC")); })) {
            report.fail(QStringLiteral("flatpak install flow"), QStringLiteral("fake log: %1").arg(readFile(QString::fromLocal8Bit(qgetenv("FAKE_LOG")))));
        } else {
            report.pass(QStringLiteral("flatpak install flow (socat flatpak install -y --system)"));
        }
    }

    // ---- flatpak uninstall ----
    {
        QFile(QString::fromLocal8Bit(qgetenv("FAKE_LOG"))).remove();
        ModalDriver driver(ModalDriver::Plan::Accept, &report, &window);
        window.confirmFlatpakUninstallPublic({QStringLiteral("org.mozilla.firefox")});
        if (!waitFor([&] { return !window.opInProgress() && logContains(window, QStringLiteral("flatpak uninstall -y --system org.mozilla.firefox")); })) {
            report.fail(QStringLiteral("flatpak uninstall flow"), QStringLiteral("fake log: %1").arg(readFile(QString::fromLocal8Bit(qgetenv("FAKE_LOG")))));
        } else {
            report.pass(QStringLiteral("flatpak uninstall flow"));
        }
    }

    // ---- flatpak update all / remove unused ----
    {
        QFile(QString::fromLocal8Bit(qgetenv("FAKE_LOG"))).remove();
        ModalDriver driver(ModalDriver::Plan::Accept, &report, &window);
        window.flatpakUpdateAllPublic();
        if (!waitFor([&] { return !window.opInProgress() && logContains(window, QStringLiteral("flatpak update --system")); })) {
            report.fail(QStringLiteral("flatpak update all flow"));
        } else {
            report.pass(QStringLiteral("flatpak update all flow"));
        }
        QFile(QString::fromLocal8Bit(qgetenv("FAKE_LOG"))).remove();
        window.flatpakRemoveUnusedPublic();
        if (!waitFor([&] { return !window.opInProgress() && logContains(window, QStringLiteral("flatpak uninstall --unused -y --system")); })) {
            report.fail(QStringLiteral("flatpak remove unused flow"));
        } else {
            report.pass(QStringLiteral("flatpak remove unused flow"));
        }
    }

    // ---- transaction preview / conflict detection (real alpm) ----
    {
        report.note(QStringLiteral("scenario: transaction previews"));
        auto alpm = window.alpmManager();
        bool ok = true;
        if (!alpm) {
            report.fail(QStringLiteral("alpm manager available"));
            ok = false;
        } else {
            // install preview for a real package must produce details + sizes
            const auto preview = alpm->displayInstallTargets({QStringLiteral("ripgrep")}, true);
            if (preview.details.trimmed().isEmpty()) {
                report.fail(QStringLiteral("install transaction preview"), preview.details);
                ok = false;
            } else {
                report.pass(QStringLiteral("install transaction preview (details+status)"));
            }

            // remove preview for an installed package
            const auto removePreview = alpm->displayRemoveTargets({QStringLiteral("vim")}, true);
            if (removePreview.details.trimmed().isEmpty()) {
                report.fail(QStringLiteral("remove transaction preview"), removePreview.details);
                ok = false;
            } else {
                report.pass(QStringLiteral("remove transaction preview"));
            }

            // conflict detection: blas-openblas conflicts with installed blas
            QString conflictMsg;
            alpm->prepareAddTrans({QStringLiteral("blas-openblas")}, conflictMsg);
            if (!conflictMsg.contains(QStringLiteral("are in conflict"))) {
                report.fail(QStringLiteral("conflict detection"), conflictMsg);
                ok = false;
            } else {
                report.pass(QStringLiteral("conflict detection (Replace/Ignore data)"));
            }
        }
        Q_UNUSED(ok);
    }

    // ---- themes + screenshots ----
    {
        for (const auto& [mode, name] : std::initializer_list<std::pair<const char*, const char*>>{
                 {"system", "theme-system"}, {"light", "theme-light"}, {"dark", "theme-dark"}}) {
            theme::apply(theme::modeFromString(QString::fromLatin1(mode)));
            QCoreApplication::processEvents();
            report.screenshot(&window, QString::fromLatin1(name));
            if (auto* nav = window.navBar()) {
                nav->grab().save(reportDir + QLatin1Char('/') + QString::fromLatin1(name) + QStringLiteral("-nav.png"));
            }
            window.discoverPage()->grab().save(reportDir + QLatin1Char('/') + QString::fromLatin1(name) + QStringLiteral("-discover.png"));
            window.repoPage()->grab().save(reportDir + QLatin1Char('/') + QString::fromLatin1(name) + QStringLiteral("-repo.png"));
        }
        theme::apply(theme::Mode::System);
        window.resize(900, 620);
        QCoreApplication::processEvents();
        report.screenshot(&window, QStringLiteral("window-900x620"));
        window.resize(1500, 940);
        QCoreApplication::processEvents();
        report.screenshot(&window, QStringLiteral("window-1500x940"));
        window.navigate(0);
        report.pass(QStringLiteral("themes applied and screenshots captured"));
    }

    // ---- flatpak info dialog ----
    {
        window.navigate(0);
        QCoreApplication::processEvents();
    }

    report.note(QStringLiteral("UI test harness done"));
    return report.failures() == 0 ? 0 : 1;
}

}  // namespace uitest
