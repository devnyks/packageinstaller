#pragma once

#include <QHash>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>

struct AppMetadata {
    QString id;
    QString name;
    QString summary;
    QString description;
    QString iconName;
    QString iconPath;
    QString iconUrl;
    QString screenshotUrl;
    QString homepage;
    QString license;
};

struct CatalogItem {
    QString id;
    QString name;
    QString summary;
    QString description;
    QString category;
    QString version;
    QString installedVersion;
    QString status;
    QString source;
    QString repo;
    QString ref;
    QString iconName;
    QString iconPath;
    QString iconUrl;
    QString screenshotUrl;
    QString homepage;
    QString license;
    QString sizeText;
    QStringList targets;
    bool installed = false;
    bool upgradable = false;
    bool runtime = false;
    bool selected = false;
    qint64 downloadSize = 0;
    qint64 installedSize = 0;
};

struct PackageSnapshot {
    QVector<CatalogItem> packages;
    QHash<QString, AppMetadata> metadata;
    QStringList installed;
    bool valid = false;
    QString error;
};

struct CuratedSpec {
    QString category;
    QStringList targets;
    QString group;
};

struct TransactionPreview {
    QString action;
    QString summary;
    QString error;
    QStringList additions;
    QStringList removals;
    QStringList conflicts;
    qint64 downloadSize = 0;
    qint64 installedSize = 0;
    qint64 removedSize = 0;
    bool success = false;
};

struct FlatpakRemote {
    QString name;
    QString url;
};

Q_DECLARE_METATYPE(PackageSnapshot)
Q_DECLARE_METATYPE(TransactionPreview)
Q_DECLARE_METATYPE(QVector<FlatpakRemote>)
