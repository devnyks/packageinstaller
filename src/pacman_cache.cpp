#include "pacman_cache.h"

PacmanCache::PacmanCache(alpm::AlpmManagerPtr manager)
    : m_manager(std::move(manager)) {
    refreshList();
}

void PacmanCache::refreshList() {
    const auto list = m_manager->getListOfPackages();

    m_upgradeCandidates.clear();
    m_candidates.clear();

    for (const auto& pkg : list) {
        if (pkg.upgradable) {
            m_upgradeCandidates << pkg.name;
        }
        m_candidates.insert(pkg.name, QStringList{pkg.version, pkg.desc});
    }
}
