#include "flatpak_model.h"

#include <QApplication>
#include <QBrush>
#include <QPalette>
#include <QSet>

FlatpakModel::FlatpakModel(QObject* parent)
    : QAbstractTableModel(parent) {}

int FlatpakModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_rows.size();
}

int FlatpakModel::columnCount(const QModelIndex&) const {
    return ColumnCount;
}

QVariant FlatpakModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }
    const auto& row = m_rows.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case Name: return row.displayName;
        case Version: return row.version;
        case Size: return row.size;
        case StatusCol: return row.status == QLatin1String("installed") ? tr("Installed") : tr("Not installed");
        default: break;
        }
        return {};
    case Qt::CheckStateRole:
        if (index.column() == Check) {
            return m_checked.contains(row.appId) ? Qt::Checked : Qt::Unchecked;
        }
        return {};
    case Qt::ForegroundRole:
        if (index.column() == Name) {
            if (row.status == QLatin1String("installed")) {
                return QBrush(QApplication::palette().color(QPalette::PlaceholderText));
            }
        }
        return {};
    case Qt::ToolTipRole: return row.appId;
    case AppIdRole: return row.appId;
    case StatusRole: return row.status;
    case StatusTextRole: return data(index, Qt::DisplayRole);
    case IsRuntimeRole: return row.isRuntime;
    default: return {};
    }
}

bool FlatpakModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (index.column() != Check || role != Qt::CheckStateRole || !index.isValid()) {
        return false;
    }
    const QString appId = m_rows.at(index.row()).appId;
    if (value.toInt() == Qt::Checked) {
        m_checked.insert(appId);
    } else {
        m_checked.remove(appId);
    }
    emit dataChanged(index, index, {Qt::CheckStateRole});
    return true;
}

Qt::ItemFlags FlatpakModel::flags(const QModelIndex& index) const {
    Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (index.column() == Check) {
        f |= Qt::ItemIsUserCheckable;
    }
    return f;
}

QVariant FlatpakModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }
    switch (section) {
    case Check: return {};
    case Name: return tr("App");
    case Version: return tr("Version");
    case Size: return tr("Size");
    case StatusCol: return tr("Status");
    default: return {};
    }
}

void FlatpakModel::setRefs(const QList<FlatpakBackend::Ref>& refs) {
    beginResetModel();
    m_refs = refs;
    m_rows.clear();
    m_rows.reserve(refs.size());
    m_checked.clear();

    // Disambiguate duplicate short names (reference `removeDuplicatesFP`):
    // duplicates render as the last two dotted components of the app id.
    QSet<QString> seen;
    QSet<QString> duplicates;
    for (const auto& ref : refs) {
        if (seen.contains(ref.shortName)) {
            duplicates.insert(ref.shortName);
        }
        seen.insert(ref.shortName);
    }

    for (const auto& ref : refs) {
        FlatpakRow row;
        row.appId = ref.appId;
        row.displayName = duplicates.contains(ref.shortName) ? ref.appId.section(QLatin1Char('.'), -2) : ref.shortName;
        row.version = ref.version;
        row.size = ref.size;
        row.status = ref.installed ? QStringLiteral("installed") : QStringLiteral("not installed");
        row.isRuntime = ref.isRuntime;
        m_rows.append(std::move(row));
    }
    endResetModel();
}

QStringList FlatpakModel::checkedAppIds() const {
    return m_checked.values();
}

bool FlatpakModel::allCheckedInstalled() const {
    if (m_checked.isEmpty()) {
        return false;
    }
    for (const auto& row : m_rows) {
        if (m_checked.contains(row.appId) && row.status != QLatin1String("installed")) {
            return false;
        }
    }
    return true;
}

bool FlatpakModel::allCheckedNotInstalled() const {
    if (m_checked.isEmpty()) {
        return false;
    }
    for (const auto& row : m_rows) {
        if (m_checked.contains(row.appId) && row.status != QLatin1String("not installed")) {
            return false;
        }
    }
    return true;
}

FlatpakFilterProxy::FlatpakFilterProxy(QObject* parent)
    : QSortFilterProxyModel(parent) {
    setDynamicSortFilter(true);
    setSortCaseSensitivity(Qt::CaseInsensitive);
}

void FlatpakFilterProxy::setFilter(Filter f) {
    m_filter = f;
    beginFilterChange();
    invalidateRowsFilter();
    endFilterChange();
}

void FlatpakFilterProxy::setSearchText(const QString& text) {
    m_search = text.trimmed();
    beginFilterChange();
    invalidateRowsFilter();
    endFilterChange();
}

bool FlatpakFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const {
    const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
    const QString status = sourceModel()->data(idx, FlatpakModel::StatusRole).toString();
    const bool isRuntime = sourceModel()->data(idx, FlatpakModel::IsRuntimeRole).toBool();
    const bool installed = status == QLatin1String("installed");

    switch (m_filter) {
    case FlatpakFilterProxy::Filter::AllInstalled:
        if (!installed) {
            return false;
        }
        break;
    case FlatpakFilterProxy::Filter::AllApps:
        if (isRuntime) {
            return false;
        }
        break;
    case FlatpakFilterProxy::Filter::AllRuntimes:
        if (!isRuntime) {
            return false;
        }
        break;
    case FlatpakFilterProxy::Filter::InstalledApps:
        if (!(installed && !isRuntime)) {
            return false;
        }
        break;
    case FlatpakFilterProxy::Filter::InstalledRuntimes:
        if (!(installed && isRuntime)) {
            return false;
        }
        break;
    case FlatpakFilterProxy::Filter::NotInstalled:
        if (installed) {
            return false;
        }
        break;
    case FlatpakFilterProxy::Filter::AllAvailable: break;
    }
    if (!m_search.isEmpty()) {
        const QString text = sourceModel()->data(idx, Qt::DisplayRole).toString() + QLatin1Char(' ')
            + sourceModel()->data(idx, Qt::ToolTipRole).toString();
        if (!text.contains(m_search, Qt::CaseInsensitive)) {
            return false;
        }
    }
    return true;
}
