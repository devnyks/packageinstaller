#include "package_model.h"

#include <QApplication>
#include <QBrush>
#include <QPalette>
#include <QIcon>



PackageModel::PackageModel(QObject* parent)
    : QAbstractTableModel(parent) {}

int PackageModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

int PackageModel::columnCount(const QModelIndex&) const {
    return ColumnCount;
}

QVariant PackageModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_rows.size())) {
        return {};
    }
    const auto& row = m_rows[static_cast<size_t>(index.row())];
    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case Name: return row.name;
        case Version: return row.version;
        case Description: return row.description;
        case StatusCol:
            switch (row.state) {
            case PackageRow::State::Installed: return tr("Installed");
            case PackageRow::State::Upgradable: return tr("Upgradable");
            case PackageRow::State::NotInstalled: return tr("Not installed");
            }
            break;
        default: break;
        }
        return {};
    case Qt::CheckStateRole:
        if (index.column() == Check) {
            return m_checked.contains(row.name) ? Qt::Checked : Qt::Unchecked;
        }
        return {};
    case Qt::DecorationRole:
        if (index.column() == Check && row.state == PackageRow::State::Upgradable) {
            return QIcon::fromTheme(QStringLiteral("software-update-available-symbolic"),
                QIcon(QStringLiteral(":/icons/software-update-available.png")));
        }
        return {};
    case Qt::ToolTipRole:
        if (index.column() == Name || index.column() == Description) {
            switch (row.state) {
            case PackageRow::State::Installed:
                return tr("Version %1 already installed").arg(row.version);
            case PackageRow::State::Upgradable:
                return tr("Version %1 installed").arg(row.version);
            case PackageRow::State::NotInstalled:
                return tr("Version %1 in repo").arg(row.version);
            }
        }
        return {};
    case Qt::ForegroundRole:
        if (index.column() == Name || index.column() == Description) {
            if (row.state != PackageRow::State::NotInstalled && row.state != PackageRow::State::Upgradable) {
                return QBrush(QApplication::palette().color(QPalette::PlaceholderText));
            }
        }
        return {};
    case CheckRole: return row.name;
    case PackageNameRole: return row.name;
    case StateRole: return static_cast<int>(row.state);
    case StateTextRole: return data(index, Qt::DisplayRole);
    case VisibleRole: return row.visible;
    case SearchTextRole: return row.name + QLatin1Char('\n') + row.description;
    default: return {};
    }
}

bool PackageModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (index.column() != Check || role != Qt::CheckStateRole || !index.isValid()) {
        return false;
    }
    auto& row = m_rows[static_cast<size_t>(index.row())];
    const bool checked = value.toInt() == Qt::Checked;
    if (checked) {
        m_checked.insert(row.name);
    } else {
        m_checked.remove(row.name);
    }
    emit dataChanged(index, index, {Qt::CheckStateRole});
    return true;
}

Qt::ItemFlags PackageModel::flags(const QModelIndex& index) const {
    Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (index.column() == Check) {
        f |= Qt::ItemIsUserCheckable;
    }
    return f;
}

QVariant PackageModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }
    switch (section) {
    case Check: return {};
    case Name: return tr("Package");
    case Version: return tr("Version");
    case Description: return tr("Description");
    case StatusCol: return tr("Status");
    default: return {};
    }
}

void PackageModel::setRows(std::vector<PackageRow> rows) {
    beginResetModel();
    m_rows = std::move(rows);
    m_checked.clear();
    m_nameIndex.clear();
    for (int i = 0; i < static_cast<int>(m_rows.size()); ++i) {
        m_nameIndex.insert(m_rows[static_cast<size_t>(i)].name, i);
    }
    endResetModel();
}

QModelIndex PackageModel::indexForName(const QString& name) const {
    const auto it = m_nameIndex.constFind(name);
    if (it == m_nameIndex.constEnd()) {
        return {};
    }
    return index(it.value(), 0);
}

QStringList PackageModel::checkedNames() const {
    return m_checked.values();
}

int PackageModel::checkedCount() const {
    return m_checked.size();
}

int PackageModel::upgradableCount() const {
    return static_cast<int>(std::count_if(m_rows.begin(), m_rows.end(),
        [](const PackageRow& r) { return r.state == PackageRow::State::Upgradable; }));
}

PackageFilterProxy::PackageFilterProxy(QObject* parent)
    : QSortFilterProxyModel(parent) {
    setDynamicSortFilter(true);
    setSortCaseSensitivity(Qt::CaseInsensitive);
}

void PackageFilterProxy::setSearchText(const QString& text) {
    m_search = text.trimmed();
    beginFilterChange();
    invalidateRowsFilter();
    endFilterChange();
}

void PackageFilterProxy::setStatusFilter(StatusFilter filter) {
    m_status = filter;
    beginFilterChange();
    invalidateRowsFilter();
    endFilterChange();
}

void PackageFilterProxy::setHideLibs(bool hide) {
    m_hideLibs = hide;
    beginFilterChange();
    invalidateRowsFilter();
    endFilterChange();
}

void PackageFilterProxy::refresh() {
    beginFilterChange();
    invalidateRowsFilter();
    endFilterChange();
}

bool PackageFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const {
    const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
    const auto visible = sourceModel()->data(idx, PackageModel::VisibleRole).toBool();
    if (!visible) {
        return false;
    }
    const QString name = sourceModel()->data(idx, PackageModel::PackageNameRole).toString();
    if (m_hideLibs && (name.startsWith(QLatin1String("lib")) && !name.startsWith(QLatin1String("libre")))) {
        return false;
    }
    if (m_hideLibs
        && (name.endsWith(QLatin1String("-dev")) || name.endsWith(QLatin1String("-dbg"))
            || name.endsWith(QLatin1String("-dbgsym")) || name.endsWith(QLatin1String("-debug"))
            || name.endsWith(QLatin1String("-devel")))) {
        return false;
    }
    if (m_status != StatusFilter::All) {
        const int state = sourceModel()->data(idx, PackageModel::StateRole).toInt();
        const bool upgradable = state == static_cast<int>(PackageRow::State::Upgradable);
        const bool installed = state != static_cast<int>(PackageRow::State::NotInstalled);
        if (m_status == StatusFilter::Upgradable && !upgradable) {
            return false;
        }
        if (m_status == StatusFilter::Installed && !installed) {
            return false;
        }
        if (m_status == StatusFilter::NotInstalled && installed) {
            return false;
        }
    }
    if (!m_search.isEmpty()) {
        const QString text = sourceModel()->data(idx, PackageModel::SearchTextRole).toString();
        if (!text.contains(m_search, Qt::CaseInsensitive)) {
            return false;
        }
    }
    return true;
}
