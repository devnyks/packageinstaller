#pragma once

#include "flatpak.h"

#include <QAbstractTableModel>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QStringList>

/// Table rows for the Flatpak page (reference `displayFlatpaks` semantics:
/// short name, long name, version, size, status; duplicate short names are
/// disambiguated with the last two dotted components).
struct FlatpakRow {
    QString appId;      // full dotted id ("org.videolan.VLC")
    QString displayName;  // short name or disambiguated name
    QString version;
    QString size;
    QString status;     // "installed" / "not installed"
    bool isRuntime = false;
};

class FlatpakModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column { Check = 0, Name, Version, Size, StatusCol, ColumnCount };
    enum Role {
        AppIdRole = Qt::UserRole,
        StatusRole,
        StatusTextRole,
        IsRuntimeRole,
    };

    explicit FlatpakModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void setRefs(const QList<FlatpakBackend::Ref>& refs);
    QStringList checkedAppIds() const;
    int checkedCount() const { return m_checked.size(); }
    bool allCheckedInstalled() const;
    bool allCheckedNotInstalled() const;
    void clearChecks() { m_checked.clear(); }

    const QList<FlatpakBackend::Ref>& refs() const { return m_refs; }

private:
    QList<FlatpakBackend::Ref> m_refs;
    QList<FlatpakRow> m_rows;
    QSet<QString> m_checked;
};

/// Flatpak filters: All available / All installed / All apps / All runtimes /
/// Installed apps / Installed runtimes / Not installed.
class FlatpakFilterProxy : public QSortFilterProxyModel {
    Q_OBJECT

public:
    enum class Filter {
        AllAvailable,
        AllInstalled,
        AllApps,
        AllRuntimes,
        InstalledApps,
        InstalledRuntimes,
        NotInstalled,
    };

    explicit FlatpakFilterProxy(QObject* parent = nullptr);

    void setFilter(Filter f);
    void setSearchText(const QString& text);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    Filter m_filter = Filter::AllAvailable;
    QString m_search;
};
