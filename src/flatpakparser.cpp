#include "flatpakparser.hpp"

QVector<FlatpakRemote> FlatpakParser::parseRemotes(const QString& output) {
    QVector<FlatpakRemote> result;
    for (const auto& line : output.split('\n', Qt::SkipEmptyParts)) {
        const auto fields = line.trimmed().split('\t', Qt::KeepEmptyParts);
        if (fields.isEmpty() || fields.first().isEmpty()) {
            continue;
        }
        result.push_back({fields.first(), fields.value(1)});
    }
    return result;
}

QVector<CatalogItem> FlatpakParser::parseRows(const QString& output, bool runtime, const QStringList& installedRefs, const QHash<QString, AppMetadata>& metadata) {
    QVector<CatalogItem> result;
    for (const auto& line : output.split('\n', Qt::SkipEmptyParts)) {
        const auto fields = line.split('\t', Qt::KeepEmptyParts);
        if (fields.isEmpty()) {
            continue;
        }
        const auto ref = fields.value(0).trimmed();
        const auto appId = ref.section('/', 1, 1);
        if (ref.isEmpty() || appId.endsWith(QStringLiteral(".Locale")) || appId.endsWith(QStringLiteral(".Debug")) || appId.endsWith(QStringLiteral(".Sources"))) {
            continue;
        }

        const auto app = metadata.value(appId);
        CatalogItem item;
        item.id = QStringLiteral("flatpak:") + ref;
        item.ref = ref;
        item.name = app.name.isEmpty() ? fields.value(3).trimmed() : app.name;
        if (item.name.isEmpty()) {
            item.name = appId;
        }
        item.summary = app.summary.isEmpty() ? fields.value(4).trimmed() : app.summary;
        item.description = app.description.isEmpty() ? item.summary : app.description;
        item.version = fields.value(1).trimmed();
        item.sizeText = fields.value(2).trimmed();
        item.source = QStringLiteral("flatpak");
        item.runtime = runtime;
        item.iconName = app.iconName.isEmpty() ? appId : app.iconName;
        item.iconPath = app.iconPath;
        item.iconUrl = app.iconUrl;
        item.screenshotUrl = app.screenshotUrl;
        item.homepage = app.homepage;
        item.license = app.license;
        item.targets = {ref};
        item.installed = installedRefs.contains(ref);
        item.status = item.installed ? QStringLiteral("installed") : QStringLiteral("available");
        result.push_back(std::move(item));
    }
    return result;
}
