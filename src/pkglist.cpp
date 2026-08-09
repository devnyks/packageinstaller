#include "pkglist.h"

#include "logging.h"

/// Minimal YAML subset parser for pkglist.yaml. The file is a list of maps:
///   - name: "Category"
///     packages:
///       - a b c
/// Optionally a `subgroups` key holds nested maps with the same shape
/// (reference `processMap` supports recursion up to depth 2).
/// This is a purpose-built parser for this exact schema — no external YAML
/// dependency (the reference used rapidyaml).

namespace {

struct Line {
    int indent = 0;
    QString text;
};

QList<Line> tokenize(const QString& src) {
    QList<Line> lines;
    for (const QString& raw : src.split(QLatin1Char('\n'))) {
        QString line = raw;
        int indent = 0;
        while (indent < line.size() && line[indent] == QLatin1Char(' ')) {
            ++indent;
        }
        line = line.mid(indent).trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        lines.append(Line{indent, line});
    }
    return lines;
}

QString stripQuotes(QString s) {
    s = s.trimmed();
    if (s.size() >= 2 && ((s.startsWith(QLatin1Char('"')) && s.endsWith(QLatin1Char('"')))
            || (s.startsWith(QLatin1Char('\'')) && s.endsWith(QLatin1Char('\''))))) {
        return s.mid(1, s.size() - 2);
    }
    return s;
}

void processMap(const QList<Line>& lines, int& idx, const QString& parentGroup, QList<PopularEntry>& out, int depth) {
    // - name: "X"  followed by indented children
    while (idx < lines.size()) {
        const Line& first = lines[idx];
        if (!first.text.startsWith(QLatin1String("- name:"))) {
            return;
        }
        const QString category = stripQuotes(first.text.mid(7));
        const int baseIndent = first.indent;
        ++idx;

        QList<QPair<QString, QStringList>> pkgLines;  // (subgroup, names)

        QString currentSubgroup;
        QStringList currentNames;
        auto flush = [&] {
            if (!currentNames.isEmpty()) {
                pkgLines.append({currentSubgroup, currentNames});
                currentNames.clear();
            }
        };

        while (idx < lines.size() && lines[idx].indent > baseIndent) {
            const Line& child = lines[idx];
            if (child.text.startsWith(QLatin1String("packages:"))) {
                ++idx;
                while (idx < lines.size() && lines[idx].indent > child.indent) {
                    const Line& item = lines[idx];
                    if (item.text.startsWith(QLatin1String("- "))) {
                        currentSubgroup.clear();
                        flush();
                        const QStringList names = item.text.mid(2).trimmed().split(QLatin1Char(' '), Qt::SkipEmptyParts);
                        pkgLines.append({QString(), names});
                    } else if (item.text.startsWith(QLatin1String("subgroups:"))) {
                        flush();
                        ++idx;
                        processMap(lines, idx, category, out, depth + 1);
                        continue;
                    }
                    ++idx;
                }
                continue;
            }
            if (child.text.startsWith(QLatin1String("subgroups:"))) {
                flush();
                ++idx;
                processMap(lines, idx, category, out, depth + 1);
                continue;
            }
            // unknown child (e.g. description) — skip
            ++idx;
        }
        flush();

        for (const auto& [subgroup, names] : pkgLines) {
            if (names.isEmpty()) {
                continue;
            }
            PopularEntry entry;
            entry.category = category;
            entry.group = subgroup.isEmpty() ? (depth > 0 ? parentGroup : category) : subgroup;
            entry.installNames = names;
            entry.uninstallNames = names;
            out.append(entry);
        }
    }
}

}  // namespace

void PkgList::parse(const QString& yamlText, QList<PopularEntry>& out) {
    const QList<Line> lines = tokenize(yamlText);
    int idx = 0;
    processMap(lines, idx, {}, out, 0);
    if (out.isEmpty()) {
        logging::warn(QStringLiteral("pkglist.yaml parsed to zero entries"));
    }
}
