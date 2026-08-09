#include "appcontroller.hpp"

#include <QGuiApplication>
#include <QIcon>
#include <QLoggingCategory>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QSharedMemory>
#include <QMessageLogContext>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <QTranslator>

#include <QFile>
#include <QDir>

#include <unistd.h>

namespace {

bool alreadyRunning(QSharedMemory& sharedMemory) {
    if (sharedMemory.create(1)) {
        return false;
    }
    if (sharedMemory.attach()) {
        sharedMemory.detach();
    }
    return !sharedMemory.create(1);
}

QString pacmanDatabasePath() {
    QProcess process;
    process.setProgram(QStringLiteral("pacman-conf"));
    process.setArguments({QStringLiteral("DBPath")});
    process.start();
    if (!process.waitForStarted(1500) || !process.waitForFinished(2500)
        || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        return QStringLiteral("/var/lib/pacman");
    }
    for (const auto& line : QString::fromLocal8Bit(process.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts)) {
        if (line.trimmed().startsWith(QStringLiteral("DBPath"))) {
            const auto path = line.section('=', 1).trimmed();
            if (!path.isEmpty()) {
                return QDir::cleanPath(path);
            }
        }
    }
    return QStringLiteral("/var/lib/pacman");
}

void initializeLogging() {
    const auto cachePath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(cachePath);
    const auto currentPath = QDir(cachePath).filePath(QStringLiteral("cachyos-catalog.log"));
    const auto oldPath = currentPath + QStringLiteral(".old");
    if (QFile::exists(currentPath)) {
        QFile::remove(oldPath);
        QFile::rename(currentPath, oldPath);
    }
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext&, const QString& message) {
        const auto path = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/cachyos-catalog.log");
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            return;
        }
        const auto level = type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg ? QStringLiteral("WARN") : QStringLiteral("INFO");
        file.write(QStringLiteral("[%1] %2\n").arg(level, message).toUtf8());
    });
}

} // namespace

int main(int argc, char* argv[]) {
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QGuiApplication::setOrganizationName(QStringLiteral("cachyos"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("cachyos.org"));
    QGuiApplication::setApplicationName(QStringLiteral("cachyos-catalog"));
    QGuiApplication::setApplicationVersion(QStringLiteral("2.0.0"));

    QGuiApplication app(argc, argv);
    if (app.arguments().contains(QStringLiteral("--help")) || app.arguments().contains(QStringLiteral("-h"))) {
        QTextStream(stdout) << "Usage: cachyos-catalog [--screenshot PATH] [--size WIDTHxHEIGHT]\n"
                               "Browse and manage CachyOS native packages and Flatpaks.\n";
        return EXIT_SUCCESS;
    }
    if (app.arguments().contains(QStringLiteral("--version"))) {
        QTextStream(stdout) << "cachyos-catalog 2.0.0\n";
        return EXIT_SUCCESS;
    }
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/resources/cachyos-catalog.svg")));
    initializeLogging();

    QTranslator translator;
    const auto locale = QLocale::system().name();
    if (translator.load(QStringLiteral(":/i18n/catalog_%1.qm").arg(locale))
        || translator.load(QStringLiteral(":/i18n/catalog_%1.qm").arg(locale.section('_', 0, 0)))) {
        app.installTranslator(&translator);
    }

    if (getuid() == 0) {
        qCritical("CachyOS Software Catalog must not be run as root");
        return EXIT_FAILURE;
    }
    if (QFile::exists(QDir(pacmanDatabasePath()).filePath(QStringLiteral("db.lck")))) {
        qCritical("Another package manager currently owns the pacman database lock");
        return EXIT_FAILURE;
    }
    const QDir syncDirectory(QDir(pacmanDatabasePath()).filePath(QStringLiteral("sync")));
    if (syncDirectory.entryList({QStringLiteral("*.db")}, QDir::Files).isEmpty()) {
        qCritical("No pacman sync databases are available");
        return EXIT_FAILURE;
    }

    QSharedMemory instanceLock(QStringLiteral("CachyOS-Catalog-instance"));
    if (alreadyRunning(instanceLock)) {
        qCritical("Another CachyOS Software Catalog instance is already running");
        return EXIT_FAILURE;
    }

    AppController controller;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("controller"), &controller);
    engine.loadFromModule(QStringLiteral("CachyOS.Catalog"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty()) {
        return EXIT_FAILURE;
    }
    const auto screenshotIndex = app.arguments().indexOf(QStringLiteral("--screenshot"));
    const auto sizeIndex = app.arguments().indexOf(QStringLiteral("--size"));
    if (sizeIndex >= 0 && sizeIndex + 1 < app.arguments().size()) {
        const auto dimensions = app.arguments().at(sizeIndex + 1).split('x');
        if (dimensions.size() == 2) {
            const int width = dimensions.at(0).toInt();
            const int height = dimensions.at(1).toInt();
            if (width > 0 && height > 0) {
                if (auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first())) {
                    window->resize(width, height);
                }
            }
        }
    }
    if (screenshotIndex >= 0 && screenshotIndex + 1 < app.arguments().size()) {
        const auto screenshotPath = app.arguments().at(screenshotIndex + 1);
        if (auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first())) {
            QTimer::singleShot(10000, &app, [window, screenshotPath, &app] {
                if (!window->grabWindow().save(screenshotPath)) {
                    qWarning().noquote() << "Unable to save screenshot:" << screenshotPath;
                }
                app.quit();
            });
        }
    }
    return app.exec();
}
