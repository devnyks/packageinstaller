#include "pacman_conf.h"

#include "logging.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

namespace {

QString stripComment(const QString& line) {
    int hash = line.indexOf('#');
    return (hash >= 0) ? line.left(hash) : line;
}

void parseServerLines(const QString& filePath, QStringList& servers) {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        logging::debug(QStringLiteral("cannot open include file %1").arg(filePath));
        return;
    }
    QTextStream ts(&f);
    static const QRegularExpression serverRe(QStringLiteral("^\\s*Server\\s*=\\s*(.+)$"));
    while (!ts.atEnd()) {
        const QString line = stripComment(ts.readLine()).trimmed();
        const auto m = serverRe.match(line);
        if (m.hasMatch()) {
            servers << m.captured(1).trimmed();
        }
    }
}

}  // namespace

PacmanConfig PacmanConfig::parse(const QString& path) {
    PacmanConfig cfg;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        logging::warn(QStringLiteral("cannot open %1").arg(path));
        return cfg;
    }

    QTextStream ts(&f);
    QString currentSection;
    auto* currentRepo = static_cast<PacmanConfig::Repo*>(nullptr);

    static const QRegularExpression sectionRe(QStringLiteral("^\\s*\\[([^\\]]+)\\]\\s*$"));
    static const QRegularExpression kvRe(QStringLiteral("^\\s*([A-Za-z0-9]+)\\s*=\\s*(.+)$"));

    while (!ts.atEnd()) {
        const QString raw = ts.readLine();
        const QString line = stripComment(raw).trimmed();
        if (line.isEmpty()) {
            continue;
        }
        const auto sec = sectionRe.match(line);
        if (sec.hasMatch()) {
            currentSection = sec.captured(1).trimmed();
            if (currentSection != QStringLiteral("options")) {
                cfg.repos.append(PacmanConfig::Repo{currentSection, {}});
                currentRepo = &cfg.repos.last();
            } else {
                currentRepo = nullptr;
            }
            continue;
        }
        const auto kv = kvRe.match(line);
        if (!kv.hasMatch()) {
            continue;
        }
        const QString key = kv.captured(1);
        const QString value = kv.captured(2).trimmed();

        if (currentRepo) {
            if (key == QStringLiteral("Server")) {
                currentRepo->servers << value;
            } else if (key == QStringLiteral("Include")) {
                parseServerLines(value, currentRepo->servers);
            }
        } else if (currentSection == QStringLiteral("options")) {
            if (key == QStringLiteral("Architecture")) {
                for (const auto& arch : value.split(QLatin1Char(' '))) {
                    cfg.architectures << arch.trimmed();
                }
            } else if (key == QStringLiteral("SigLevel")) {
                cfg.sigLevel = value;
            }
        }
    }
    return cfg;
}
