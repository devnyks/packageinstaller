#pragma once

#include "types.hpp"

#include <QString>

struct _alpm_handle_t;
using alpm_handle_t = _alpm_handle_t;

class AlpmBackend final {
public:
    AlpmBackend() = default;
    ~AlpmBackend();

    AlpmBackend(const AlpmBackend&) = delete;
    AlpmBackend& operator=(const AlpmBackend&) = delete;

    bool initialize(QString& error);
    bool reload(QString& error);
    PackageSnapshot loadSnapshot();
    TransactionPreview previewInstall(const QStringList& targets);
    TransactionPreview previewRemove(const QStringList& targets);
    TransactionPreview previewSystemUpgrade();

private:
    static bool copyTree(const QString& source, const QString& destination, QString& error);
    static QString commandOutput(const QStringList& arguments, QString& error);
    static QString parseDirective(const QString& output, const QString& name);
    static QStringList parseDirectiveList(const QString& output, const QString& name);
    static QString formatSize(qint64 bytes);

    QHash<QString, AppMetadata> loadAppStream() const;
    TransactionPreview preview(const QStringList& targets, const QString& action, bool remove, bool systemUpgrade);

    alpm_handle_t* m_handle = nullptr;
    QString m_sourceDbPath;
    QString m_snapshotDbPath;
    QHash<QString, AppMetadata> m_metadata;
    bool m_metadataLoaded = false;
};
