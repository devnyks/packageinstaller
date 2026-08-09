#include "catalogmodel.hpp"

#include <QSet>

#include <algorithm>

CatalogModel::CatalogModel(QObject* parent)
    : QAbstractListModel(parent) {
}

int CatalogModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_visible.size();
}

QVariant CatalogModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_visible.size()) {
        return {};
    }

    const auto& item = m_items.at(m_visible.at(index.row()));
    switch (role) {
    case ItemIdRole: return item.id;
    case NameRole: return item.name;
    case SummaryRole: return item.summary;
    case DescriptionRole: return item.description;
    case CategoryRole: return item.category;
    case VersionRole: return item.version;
    case InstalledVersionRole: return item.installedVersion;
    case StatusRole: return item.status;
    case SourceRole: return item.source;
    case RepoRole: return item.repo;
    case RefRole: return item.ref;
    case IconNameRole: return item.iconName;
    case IconPathRole: return item.iconPath;
    case IconUrlRole: return item.iconUrl;
    case ScreenshotRole: return item.screenshotUrl;
    case HomepageRole: return item.homepage;
    case LicenseRole: return item.license;
    case SizeTextRole: return item.sizeText;
    case TargetsRole: return item.targets;
    case InstalledRole: return item.installed;
    case UpgradableRole: return item.upgradable;
    case RuntimeRole: return item.runtime;
    case SelectedRole: return item.selected;
    case DownloadSizeRole: return item.downloadSize;
    case InstalledSizeRole: return item.installedSize;
    default: return {};
    }
}

QHash<int, QByteArray> CatalogModel::roleNames() const {
    return {
        {ItemIdRole, "itemId"},
        {NameRole, "name"},
        {SummaryRole, "summary"},
        {DescriptionRole, "description"},
        {CategoryRole, "category"},
        {VersionRole, "version"},
        {InstalledVersionRole, "installedVersion"},
        {StatusRole, "status"},
        {SourceRole, "source"},
        {RepoRole, "repo"},
        {RefRole, "ref"},
        {IconNameRole, "iconName"},
        {IconPathRole, "iconPath"},
        {IconUrlRole, "iconUrl"},
        {ScreenshotRole, "screenshotUrl"},
        {HomepageRole, "homepage"},
        {LicenseRole, "license"},
        {SizeTextRole, "sizeText"},
        {TargetsRole, "targets"},
        {InstalledRole, "installed"},
        {UpgradableRole, "upgradable"},
        {RuntimeRole, "runtime"},
        {SelectedRole, "selected"},
        {DownloadSizeRole, "downloadSize"},
        {InstalledSizeRole, "installedSize"},
    };
}

void CatalogModel::setItems(QVector<CatalogItem> items) {
    const QSet<QString> selectedIds = [this] {
        QSet<QString> result;
        for (const auto& item : m_items) {
            if (item.selected) {
                result.insert(item.id);
            }
        }
        return result;
    }();

    for (auto& item : items) {
        item.selected = selectedIds.contains(item.id);
    }

    m_items = std::move(items);
    rebuildVisible();
    emit totalCountChanged();
    emit selectionChanged();
}

QVector<CatalogItem> CatalogModel::selectedItems() const {
    QVector<CatalogItem> result;
    for (const auto& item : m_items) {
        if (item.selected) {
            result.push_back(item);
        }
    }
    return result;
}

const CatalogItem* CatalogModel::itemAt(int row) const {
    if (row < 0 || row >= m_visible.size()) {
        return nullptr;
    }
    return &m_items.at(m_visible.at(row));
}

int CatalogModel::selectedCount() const {
    return std::count_if(m_items.cbegin(), m_items.cend(), [](const auto& item) { return item.selected; });
}

void CatalogModel::toggle(int row) {
    if (row < 0 || row >= m_visible.size()) {
        return;
    }
    const int sourceRow = m_visible.at(row);
    m_items[sourceRow].selected = !m_items[sourceRow].selected;
    const auto modelIndex = index(row, 0);
    emit dataChanged(modelIndex, modelIndex, {SelectedRole});
    emit selectionChanged();
}

void CatalogModel::clearSelection() {
    bool changed = false;
    for (auto& item : m_items) {
        changed = changed || item.selected;
        item.selected = false;
    }
    if (changed) {
        if (!m_visible.isEmpty()) {
            emit dataChanged(index(0, 0), index(m_visible.size() - 1, 0), {SelectedRole});
        }
        emit selectionChanged();
    }
}

void CatalogModel::selectAllVisible() {
    bool changed = false;
    for (const int sourceRow : m_visible) {
        changed = changed || !m_items[sourceRow].selected;
        m_items[sourceRow].selected = true;
    }
    if (changed) {
        if (!m_visible.isEmpty()) {
            emit dataChanged(index(0, 0), index(m_visible.size() - 1, 0), {SelectedRole});
        }
        emit selectionChanged();
    }
}

void CatalogModel::setSearch(const QString& value) {
    const auto normalized = value.trimmed();
    if (m_search == normalized) {
        return;
    }
    m_search = normalized;
    rebuildVisible();
    emit searchChanged();
}

void CatalogModel::setStatusFilter(const QString& value) {
    const auto normalized = value.trimmed().toLower();
    if (m_statusFilter == normalized) {
        return;
    }
    m_statusFilter = normalized;
    rebuildVisible();
    emit statusFilterChanged();
}

void CatalogModel::setHideLibraries(bool value) {
    if (m_hideLibraries == value) {
        return;
    }
    m_hideLibraries = value;
    rebuildVisible();
    emit hideLibrariesChanged();
}

void CatalogModel::rebuildVisible() {
    beginResetModel();
    m_visible.clear();
    m_visible.reserve(m_items.size());
    bool selectionCleared = false;
    for (int row = 0; row < m_items.size(); ++row) {
        if (matches(m_items.at(row))) {
            m_visible.push_back(row);
        } else if (m_items[row].selected) {
            m_items[row].selected = false;
            selectionCleared = true;
        }
    }
    endResetModel();
    emit countChanged();
    if (selectionCleared) {
        emit selectionChanged();
    }
}

bool CatalogModel::isLibraryOrDevelopmentPackage(const QString& name) {
    const auto lower = name.toLower();
    return (lower.startsWith(QStringLiteral("lib")) && !lower.startsWith(QStringLiteral("libre")))
        || lower.endsWith(QStringLiteral("-dev"))
        || lower.endsWith(QStringLiteral("-devel"))
        || lower.endsWith(QStringLiteral("-debug"))
        || lower.endsWith(QStringLiteral("-dbg"))
        || lower.endsWith(QStringLiteral("-dbgsym"));
}

bool CatalogModel::matches(const CatalogItem& item) const {
    const auto packageName = item.targets.isEmpty() ? item.name : item.targets.first();
    if (m_hideLibraries && item.source == QStringLiteral("native") && isLibraryOrDevelopmentPackage(packageName)) {
        return false;
    }

    if (m_statusFilter == QStringLiteral("installed") || m_statusFilter == QStringLiteral("all-installed")) {
        if (!item.installed) {
            return false;
        }
    } else if (m_statusFilter == QStringLiteral("upgradable")) {
        if (!item.upgradable) {
            return false;
        }
    } else if (m_statusFilter == QStringLiteral("not installed") || m_statusFilter == QStringLiteral("available")) {
        if (item.installed) {
            return false;
        }
    } else if (m_statusFilter == QStringLiteral("apps")) {
        if (item.runtime) {
            return false;
        }
    } else if (m_statusFilter == QStringLiteral("runtimes")) {
        if (!item.runtime) {
            return false;
        }
    }

    if (m_search.isEmpty()) {
        return true;
    }
    const auto needle = m_search.toCaseFolded();
    return item.name.toCaseFolded().contains(needle)
        || packageName.toCaseFolded().contains(needle)
        || item.summary.toCaseFolded().contains(needle)
        || item.description.toCaseFolded().contains(needle)
        || item.category.toCaseFolded().contains(needle)
        || item.repo.toCaseFolded().contains(needle)
        || item.ref.toCaseFolded().contains(needle);
}
