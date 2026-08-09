#pragma once

#include "types.hpp"

class FlatpakParser final {
public:
    static QVector<FlatpakRemote> parseRemotes(const QString& output);
    static QVector<CatalogItem> parseRows(const QString& output, bool runtime, const QStringList& installedRefs, const QHash<QString, AppMetadata>& metadata);
};
