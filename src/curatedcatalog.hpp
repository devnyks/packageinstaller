#pragma once

#include "types.hpp"

#include <QByteArray>

class CuratedCatalog final {
public:
    static QVector<CuratedSpec> parse(const QByteArray& yaml);
};
