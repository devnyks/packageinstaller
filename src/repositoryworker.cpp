#include "repositoryworker.hpp"

RepositoryWorker::RepositoryWorker(QObject* parent)
    : QObject(parent) {
}

void RepositoryWorker::load() {
    QString error;
    if (!m_backend.reload(error)) {
        emit fatalError(error);
        return;
    }
    const auto snapshot = m_backend.loadSnapshot();
    if (!snapshot.valid) {
        emit fatalError(snapshot.error);
        return;
    }
    emit snapshotReady(snapshot);
}

void RepositoryWorker::previewInstall(const QStringList& targets) {
    emit previewReady(m_backend.previewInstall(targets));
}

void RepositoryWorker::previewRemove(const QStringList& targets) {
    emit previewReady(m_backend.previewRemove(targets));
}

void RepositoryWorker::previewSystemUpgrade() {
    emit previewReady(m_backend.previewSystemUpgrade());
}
