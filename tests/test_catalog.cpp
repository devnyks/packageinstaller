#include "alpmbackend.hpp"
#include "appcontroller.hpp"
#include "catalogmodel.hpp"
#include "curatedcatalog.hpp"
#include "flatpakparser.hpp"
#include "operationcommands.hpp"
#include "processrunner.hpp"

#include <QFile>
#include <QDir>
#include <QProcess>
#include <QSignalSpy>
#include <QtTest>

class CatalogTests final : public QObject {
    Q_OBJECT

private slots:
    void parsesCuratedEntries();
    void filtersAndSelectsItems();
    void enumeratesPacmanMetadataWithoutMutation();
    void parsesFlatpakRowsAndRemotes();
    void readsFlatpakStateWithoutMutation();
    void loadsControllerFlatpakStateReadOnly();
    void buildsArgvWithoutShellInterpolation();
    void reportsFailedProcessStart();
    void reportsCancellation();
};

void CatalogTests::parsesCuratedEntries() {
    const QByteArray yaml = R"yaml(
- name: "Browsers"
  packages:
    - firefox
    - vivaldi vivaldi-ffmpeg-codecs
- name: "Office"
  packages:
    - libreoffice-fresh libmythes
)yaml";
    const auto entries = CuratedCatalog::parse(yaml);
    QCOMPARE(entries.size(), 3);
    QCOMPARE(entries.at(0).category, QStringLiteral("Browsers"));
    QCOMPARE(entries.at(1).targets, QStringList({QStringLiteral("vivaldi"), QStringLiteral("vivaldi-ffmpeg-codecs")}));
    QCOMPARE(entries.at(2).category, QStringLiteral("Office"));

    const auto nested = CuratedCatalog::parse(R"yaml(
- name: "Media"
  subgroups:
    - name: "Audio"
      packages:
        - strawberry
    - name: "Video"
      packages:
        - vlc
)yaml");
    QCOMPARE(nested.size(), 2);
    QCOMPARE(nested.at(0).group, QStringLiteral("Media"));
    QCOMPARE(nested.at(1).category, QStringLiteral("Video"));
}

void CatalogTests::filtersAndSelectsItems() {
    CatalogModel model;
    QVector<CatalogItem> items;
    CatalogItem installed;
    installed.id = QStringLiteral("native:firefox");
    installed.name = QStringLiteral("Firefox");
    installed.summary = QStringLiteral("Browser");
    installed.source = QStringLiteral("native");
    installed.installed = true;
    installed.status = QStringLiteral("installed");
    installed.targets = {QStringLiteral("firefox")};
    CatalogItem library;
    library.id = QStringLiteral("native:libfoo");
    library.name = QStringLiteral("libfoo");
    library.source = QStringLiteral("native");
    library.status = QStringLiteral("available");
    library.targets = {QStringLiteral("libfoo")};
    CatalogItem update;
    update.id = QStringLiteral("native:vim");
    update.name = QStringLiteral("Vim");
    update.source = QStringLiteral("native");
    update.upgradable = true;
    update.installed = true;
    update.status = QStringLiteral("upgradable");
    update.targets = {QStringLiteral("vim")};
    items << installed << library << update;
    model.setItems(items);

    QCOMPARE(model.totalCount(), 3);
    QCOMPARE(model.count(), 2);
    model.setHideLibraries(false);
    QCOMPARE(model.count(), 3);
    model.setStatusFilter(QStringLiteral("upgradable"));
    QCOMPARE(model.count(), 1);
    model.toggle(0);
    QCOMPARE(model.selectedCount(), 1);
    QCOMPARE(model.selectedItems().first().targets, QStringList({QStringLiteral("vim")}));
    model.clearSelection();
    QCOMPARE(model.selectedCount(), 0);
}

void CatalogTests::enumeratesPacmanMetadataWithoutMutation() {
    AlpmBackend backend;
    QString error;
    if (!backend.initialize(error)) {
        QSKIP(qPrintable(QStringLiteral("pacman metadata unavailable: ") + error));
    }
    const auto snapshot = backend.loadSnapshot();
    QVERIFY2(snapshot.valid, qPrintable(snapshot.error));
    QVERIFY(!snapshot.packages.isEmpty());
    QVERIFY(!snapshot.installed.isEmpty());
    const auto target = snapshot.packages.first().targets.first();
    const auto preview = backend.previewInstall({target});
    QVERIFY2(preview.success, qPrintable(preview.error));
    const auto removePreview = backend.previewRemove({snapshot.installed.first()});
    QVERIFY(removePreview.success || !removePreview.error.isEmpty());
    const auto upgrade = backend.previewSystemUpgrade();
    QVERIFY2(upgrade.success, qPrintable(upgrade.error));
    QVERIFY(QFile::exists(QStringLiteral("/var/lib/pacman/local")));
    QVERIFY(!QFile::exists(QStringLiteral("/var/lib/pacman/db.lck")));
}

void CatalogTests::parsesFlatpakRowsAndRemotes() {
    const auto remotes = FlatpakParser::parseRemotes(QStringLiteral("flathub\thttps://dl.flathub.org/repo/\ncustom\thttps://example.org/repo/\n"));
    QCOMPARE(remotes.size(), 2);
    QCOMPARE(remotes.at(0).name, QStringLiteral("flathub"));
    QHash<QString, AppMetadata> metadata;
    AppMetadata app;
    app.id = QStringLiteral("org.example.Editor");
    app.name = QStringLiteral("Example Editor");
    app.summary = QStringLiteral("Edit text");
    app.iconName = QStringLiteral("accessories-text-editor");
    metadata.insert(app.id, app);
    const auto rows = FlatpakParser::parseRows(
        QStringLiteral("app/org.example.Editor/x86_64/stable\t1.0\t12 MiB\tRaw name\tRaw summary\n"
                       "app/org.example.Editor.Locale/x86_64/stable\t1.0\t1 MiB\tLocale\tLocale\n"),
        false, {QStringLiteral("app/org.example.Editor/x86_64/stable")}, metadata);
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().name, QStringLiteral("Example Editor"));
    QVERIFY(rows.first().installed);
    QCOMPARE(rows.first().iconName, QStringLiteral("accessories-text-editor"));
}

void CatalogTests::readsFlatpakStateWithoutMutation() {
    QProcess process;
    process.start(QStringLiteral("flatpak"), {QStringLiteral("--system"), QStringLiteral("remote-list"), QStringLiteral("--columns=name,url")});
    if (!process.waitForStarted(2000) || !process.waitForFinished(10000)) {
        QSKIP("flatpak is unavailable");
    }
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);
    const auto remotes = FlatpakParser::parseRemotes(QString::fromLocal8Bit(process.readAllStandardOutput()));
    QVERIFY(!remotes.isEmpty());
    QVERIFY(QFile::exists(QStringLiteral("/var/lib/flatpak")) || QFile::exists(QDir::homePath() + QStringLiteral("/.local/share/flatpak")));
}

void CatalogTests::loadsControllerFlatpakStateReadOnly() {
    AppController controller;
    QTRY_VERIFY_WITH_TIMEOUT(controller.ready(), 30000);
    QVERIFY(controller.popularModel()->totalCount() > 0);
    if (!controller.flatpakAvailable()) {
        QSKIP("flatpak is unavailable");
    }
    controller.dismissFlatpakWarning(true);
    controller.loadFlatpaks();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.flatpakLoading() && !controller.operationRunning(), 30000);
    QVERIFY(!controller.remoteNames().isEmpty());
}

void CatalogTests::buildsArgvWithoutShellInterpolation() {
    const auto hostile = QStringLiteral("name; touch /tmp/should-not-exist && echo $HOME");
    const auto nativeInstall = OperationCommands::nativeMutation(QStringLiteral("install"), {hostile});
    QCOMPARE(nativeInstall.program, QStringLiteral("pkexec"));
    QCOMPARE(nativeInstall.arguments, QStringList({QStringLiteral("pacman"), QStringLiteral("-S"), hostile}));
    QVERIFY(!nativeInstall.arguments.contains(QStringLiteral("-c")));

    const auto nativeUpgrade = OperationCommands::nativeMutation(QStringLiteral("upgrade"), {QStringLiteral("vim")});
    QCOMPARE(nativeUpgrade.arguments, QStringList({QStringLiteral("pacman"), QStringLiteral("-S"), QStringLiteral("vim")}));
    const auto fullUpgrade = OperationCommands::nativeMutation(QStringLiteral("upgrade"), {});
    QCOMPARE(fullUpgrade.arguments, QStringList({QStringLiteral("pacman"), QStringLiteral("-Syu")}));

    const auto systemFlatpak = OperationCommands::flatpakMutation(QStringLiteral("system"), QStringLiteral("install"), QStringLiteral("remote;bad"), {hostile});
    QCOMPARE(systemFlatpak.program, QStringLiteral("pkexec"));
    QCOMPARE(systemFlatpak.arguments, QStringList({QStringLiteral("flatpak"), QStringLiteral("--system"), QStringLiteral("install"), QStringLiteral("-y"), QStringLiteral("remote;bad"), hostile}));
    const auto userRemote = OperationCommands::flatpakRemoteAdd(QStringLiteral("user"), QStringLiteral("custom"), QStringLiteral("https://example.org/repo.flatpakrepo"));
    QCOMPARE(userRemote.program, QStringLiteral("flatpak"));
    QCOMPARE(userRemote.arguments, QStringList({QStringLiteral("--user"), QStringLiteral("remote-add"), QStringLiteral("--if-not-exists"), QStringLiteral("custom"), QStringLiteral("https://example.org/repo.flatpakrepo")}));
}

void CatalogTests::reportsFailedProcessStart() {
    ProcessRunner runner;
    QSignalSpy completed(&runner, &ProcessRunner::completed);
    QVERIFY(runner.start(QStringLiteral("/definitely/missing/cachyos-catalog-process"), {}));
    QTRY_COMPARE_WITH_TIMEOUT(completed.count(), 1, 3000);
    const auto arguments = completed.takeFirst();
    QCOMPARE(arguments.at(0).toInt(), -1);
    QVERIFY(!arguments.at(3).toBool());
}

void CatalogTests::reportsCancellation() {
    ProcessRunner runner;
    QSignalSpy completed(&runner, &ProcessRunner::completed);
    QVERIFY(runner.start(QStringLiteral("/bin/sleep"), {QStringLiteral("5")}));
    QTRY_VERIFY_WITH_TIMEOUT(runner.running(), 1000);
    runner.cancel();
    QTRY_COMPARE_WITH_TIMEOUT(completed.count(), 1, 5000);
    const auto arguments = completed.takeFirst();
    QVERIFY(arguments.at(3).toBool());
}

QTEST_MAIN(CatalogTests)
#include "test_catalog.moc"
