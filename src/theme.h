#pragma once

#include <QColor>
#include <QString>

/// Fluent 2-inspired theme engine. Applies a light or dark palette plus a
/// generated stylesheet to the whole application. Mode follows the system
/// by default and can be overridden persistently (QSettings "theme").
namespace theme {

enum class Mode { System, Light, Dark };

Mode modeFromString(const QString& s);
QString modeToString(Mode m);

/// Apply the given mode (System resolves to the platform palette).
void apply(Mode mode);

/// The stylesheet string for the current palette (used by tests to snapshot).
QString stylesheet();

}  // namespace theme
