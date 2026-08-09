#include "logging.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QStandardPaths>
#include <QTextStream>

namespace {

QFile* g_logFile = nullptr;
QTextStream* g_stream = nullptr;
QMutex g_mutex;

const char* levelTag(int level) {
    switch (level) {
    case 0: return "DBG";
    case 1: return "INF";
    case 2: return "WRN";
    default: return "ERR";
    }
}

void writeLog(int level, const QString& msg) {
    if (!g_stream) {
        return;
    }
    QMutexLocker lock(&g_mutex);
    *g_stream << QDateTime::currentDateTime().toString("HH:mm:ss.zzz") << " [" << levelTag(level)
              << "] " << msg << '\n';
    g_stream->flush();
}

}  // namespace

namespace logging {

void init() {
    if (g_stream) {
        return;
    }
    const auto cachePath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(cachePath);
    const QString logPath = cachePath + QStringLiteral("/cachyospi.log");

    // rotate previous session (reference parity: keep one .old)
    QFile old(QStringLiteral("%1.old").arg(logPath));
    if (old.exists()) {
        old.remove();
    }
    QFile cur(logPath);
    if (cur.exists()) {
        cur.copy(QStringLiteral("%1.old").arg(logPath));
        cur.remove();
    }

    g_logFile = new QFile(logPath);
    if (g_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        g_stream = new QTextStream(g_logFile);
        writeLog(1, QStringLiteral("CACHYOS-PI SESSION START"));
    }
}

void error(const QString& msg) { writeLog(3, msg); }
void warn(const QString& msg) { writeLog(2, msg); }
void info(const QString& msg) { writeLog(1, msg); }
void debug(const QString& msg) { writeLog(0, msg); }

}  // namespace logging
