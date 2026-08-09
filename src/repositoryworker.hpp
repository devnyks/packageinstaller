#pragma once

#include "alpmbackend.hpp"

#include <QObject>

class RepositoryWorker final : public QObject {
    Q_OBJECT

public:
    explicit RepositoryWorker(QObject* parent = nullptr);

public slots:
    void load();
    void previewInstall(const QStringList& targets);
    void previewRemove(const QStringList& targets);
    void previewSystemUpgrade();

signals:
    void snapshotReady(const PackageSnapshot& snapshot);
    void previewReady(const TransactionPreview& preview);
    void fatalError(const QString& message);

private:
    AlpmBackend m_backend;
};
