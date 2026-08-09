#include "curatedcatalog.hpp"

#include <QRegularExpression>

namespace {

QString valueAfterColon(const QString& line) {
    const int colon = line.indexOf(':');
    if (colon < 0) {
        return {};
    }
    auto value = line.mid(colon + 1).trimmed();
    if (value.startsWith('#')) {
        return {};
    }
    if (value.startsWith('"') && value.endsWith('"') && value.size() >= 2) {
        value = value.mid(1, value.size() - 2);
    } else if (value.startsWith('\'') && value.endsWith('\'') && value.size() >= 2) {
        value = value.mid(1, value.size() - 2);
    }
    return value.trimmed();
}

QStringList parseTargets(QString value) {
    const int comment = value.indexOf(" #");
    if (comment >= 0) {
        value = value.left(comment);
    }
    value = value.trimmed();
    if (value.startsWith('"') && value.endsWith('"') && value.size() >= 2) {
        value = value.mid(1, value.size() - 2);
    }
    return value.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
}

} // namespace

QVector<CuratedSpec> CuratedCatalog::parse(const QByteArray& yaml) {
    QVector<CuratedSpec> result;
    struct Frame {
        int indent = 0;
        QString name;
    };
    QVector<Frame> frames;
    bool inPackages = false;

    const auto lines = QString::fromUtf8(yaml).split('\n');
    for (const auto& rawLine : lines) {
        const int indent = rawLine.indexOf(QRegularExpression(QStringLiteral("\\S")));
        const auto line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }

        if (line.startsWith("- name:")) {
            while (!frames.isEmpty() && frames.last().indent >= indent) {
                frames.removeLast();
            }
            frames.push_back({indent, valueAfterColon(line.mid(2))});
            inPackages = false;
            continue;
        }
        if (line == QStringLiteral("packages:")) {
            inPackages = true;
            continue;
        }
        if (inPackages && line.startsWith("- ")) {
            const auto targets = parseTargets(line.mid(2));
            if (!frames.isEmpty() && !frames.last().name.isEmpty() && !targets.isEmpty()) {
                result.push_back({frames.last().name, targets, frames.size() > 1 ? frames.first().name : QString()});
            }
        }
    }
    return result;
}
