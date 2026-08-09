#pragma once

#include <QString>
#include <QStringList>

/// Curated application list ("Popular" tab data source).
/// The YAML structure mirrors the reference pkglist.yaml:
///   - name: "Category"
///     packages:
///       - pkg1 pkg2
/// Optionally nested `subgroups` (reference `processMap` recursion).
struct PopularCategory {
    QString name;  // category
    QString group; // parent group (empty for top-level)
};

struct PopularEntry {
    QString category;
    QString group;
    QStringList installNames;   // all package names (install)
    QStringList uninstallNames; // all package names (uninstall)
};

class PkgList {
public:
    /// Parse a pkglist.yaml document into category/entries.
    static void parse(const QString& yamlText, QList<PopularEntry>& out);
};
