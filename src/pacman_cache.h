#pragma once

#include "alpm_manager.h"

#include <QMap>
#include <QStringList>

/// Snapshot of sync-db package data (reference `PacmanCache`):
///  - candidates:      name -> [version, description]
///  - upgradeCandidates: names of installed packages with a newer repo version
/// The snapshot is produced off the UI thread (see `buildPackageLists`).
class PacmanCache {
public:
    explicit PacmanCache(alpm::AlpmManagerPtr manager);

    void refreshList();

    const QMap<QString, QStringList>& candidates() const { return m_candidates; }
    const QStringList& upgradeCandidates() const { return m_upgradeCandidates; }

private:
    QStringList m_upgradeCandidates;
    QMap<QString, QStringList> m_candidates;
    alpm::AlpmManagerPtr m_manager;
};
