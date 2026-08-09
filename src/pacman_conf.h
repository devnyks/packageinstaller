#pragma once

#include <QString>
#include <QStringList>

/// Minimal /etc/pacman.conf parser covering the directives libalpm needs
/// to set up sync databases for read-only transactions:
///  - `[repo]` sections and their `Server =`/`Include =` lines (one level),
///  - `Architecture =` and `SigLevel` for the handle.
/// This replicates the pacmanconf step of the reference Rust backend.
struct PacmanConfig {
    struct Repo {
        QString name;
        QStringList servers;
    };

    QStringList architectures;  // empty -> alpm default
    QString sigLevel;           // empty -> alpm default
    QList<Repo> repos;

    static PacmanConfig parse(const QString& path);
};
