#pragma once

#include <QAbstractTableModel>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QStringList>

#include <algorithm>
#include <vector>

/// One row of the repository package table (reference `displayPackages`
/// semantics: name, version, description, status, filtered state).
struct PackageRow {
    enum class State { NotInstalled, Installed, Upgradable };

    QString name;
    QString version;
    QString description;
    State state = State::NotInstalled;
    bool hiddenByLibFilter = false;  // lib/-dev etc. filtering
    bool visible = true;             // status-filter + search visibility
};

class PackageModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column { Check = 0, Name, Version, Description, StatusCol, ColumnCount };
    enum Role {
        CheckRole = Qt::UserRole,
        PackageNameRole,
        StateRole,
        StateTextRole,
        VisibleRole,
        SearchTextRole,
    };

    explicit PackageModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void setRows(std::vector<PackageRow> rows);
    const std::vector<PackageRow>& rows() const { return m_rows; }
    int rowIndex(const QModelIndex& index) const { return index.row(); }

    QModelIndex indexForName(const QString& name) const;
    QStringList checkedNames() const;
    int checkedCount() const;
    int upgradableCount() const;
    template <typename Pred>
    int countOf(Pred&& pred) const {
        return static_cast<int>(std::count_if(m_rows.begin(), m_rows.end(), pred));
    }
    void clearChecks() { m_checked.clear(); }

private:
    std::vector<PackageRow> m_rows;
    QSet<QString> m_checked;
    mutable QHash<QString, int> m_nameIndex;
};

/// Filters by search text, status combo and the lib/dev filter.
class PackageFilterProxy : public QSortFilterProxyModel {
    Q_OBJECT

public:
    enum class StatusFilter { All, Upgradable, Installed, NotInstalled };

    explicit PackageFilterProxy(QObject* parent = nullptr);

    void setSearchText(const QString& text);
    void setStatusFilter(StatusFilter filter);
    void setHideLibs(bool hide);
    void refresh();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QString m_search;
    StatusFilter m_status = StatusFilter::All;
    bool m_hideLibs = false;
};
