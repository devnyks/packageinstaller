#include "alpm_manager.h"
#include "logging.h"
#include "theme.h"
#include "ui/mainwindow.h"
#include "ui_test_harness.h"

#include <QApplication>
#include <QIcon>
#include <QLibraryInfo>
#include <QLocale>
#include <QMessageBox>
#include <QSharedMemory>
#include <QSettings>
#include <QTranslator>

#include <unistd.h>

#include <filesystem>
#include <fstream>

namespace {

bool isInstanceAlreadyRunning(QSharedMemory& memoryLock) {
    if (!memoryLock.create(1)) {
        memoryLock.attach();
        memoryLock.detach();
        if (!memoryLock.create(1)) {
            return true;
        }
    }
    return false;
}

void initTranslations(QTranslator& qtTranslator, QTranslator& translator) {
    const QString lang = QLocale::system().name().section(QLatin1Char('_'), 0, 0);
    const QString langTerritory = QLocale::system().name();
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    const auto translationPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
#else
    const auto translationPath = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
#endif
    if (qtTranslator.load("qt_" + lang, translationPath) || qtTranslator.load("qt_" + langTerritory, translationPath)) {
        QApplication::installTranslator(&qtTranslator);
    }
    if (translator.load(lang, QStringLiteral(":/translations/"))
        || translator.load(langTerritory, QStringLiteral(":/translations/"))) {
        QApplication::installTranslator(&translator);
    }
}

}  // namespace

int main(int argc, char** argv) {
    QApplication::setOrganizationName(QStringLiteral("cachyos"));
    QApplication::setOrganizationDomain(QStringLiteral("cachyos.org"));
    QApplication::setApplicationName(QStringLiteral("cachyos-pi"));

    const QApplication app(argc, argv);
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/app-icon-256.png")));

    QTranslator qtTranslator;
    QTranslator translator;
    initTranslations(qtTranslator, translator);

    // single instance (same lock key as the reference application)
    QSharedMemory sharedMemoryLock(QStringLiteral("CachyOS-PI-lock"));
    if (isInstanceAlreadyRunning(sharedMemoryLock)) {
        QMessageBox::critical(nullptr, QObject::tr("Error"),
            QObject::tr("Instance of the program is already running! Please close it first"));
        return EXIT_FAILURE;
    }

    // valid package databases required (reference parity)
    if (!alpm::AlpmManager::isValidAlpmDbs()) {
        QMessageBox::critical(nullptr, QObject::tr("Error"),
            QObject::tr("No db found!\nPlease run `pacman -Sy` to update DB!\nThis is needed for the app to work properly"));
        return EXIT_FAILURE;
    }

    // root guard (reference parity)
    if (system("logname |grep -q ^root$") == 0 || getuid() == 0) {
        QMessageBox::critical(nullptr, QObject::tr("Unable to run the app"),
            QObject::tr("Please don't run that application as root user!"));
        return EXIT_FAILURE;
    }

    // pacman lock guard (reference parity)
    if (std::filesystem::exists("/var/lib/pacman/db.lck")) {
        QMessageBox::critical(nullptr, QObject::tr("Unable to get exclusive lock"),
            QObject::tr("Another package management application (like pamac or pacman), "
                        "is already running. Please close that application first"));
        return EXIT_FAILURE;
    }

    logging::init();

    // persistent theme preference (System is the default)
    const QString themeMode = QSettings().value(QStringLiteral("theme"), QStringLiteral("system")).toString();
    theme::apply(theme::modeFromString(themeMode));

    MainWindow w;
    w.show();

    // scripted end-to-end tests (see tests/run_tests.sh)
    if (qEnvironmentVariableIsSet("CACHYOS_PI_UI_TEST")) {
        const QString reportDir = QString::fromLocal8Bit(qgetenv("CACHYOS_PI_UI_TEST"));
        return uitest::run(w, reportDir);
    }

    return QApplication::exec();
}
