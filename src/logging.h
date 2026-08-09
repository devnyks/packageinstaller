#pragma once

#include <QString>
#include <QStringList>

namespace logging {

/// Initialize file logging to ~/.cache/cachyos-pi/cachyospi.log.
/// Rotates the previous session to cachyospi.log.old (reference parity).
void init();

/// Direct file logging helpers used by the ALPM callbacks.
void error(const QString& msg);
void warn(const QString& msg);
void info(const QString& msg);
void debug(const QString& msg);

}  // namespace logging
