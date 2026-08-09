#pragma once

#include "types.hpp"

#include <QAbstractListModel>

class CatalogModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY searchChanged)
    Q_PROPERTY(QString statusFilter READ statusFilter WRITE setStatusFilter NOTIFY statusFilterChanged)
    Q_PROPERTY(bool hideLibraries READ hideLibraries WRITE setHideLibraries NOTIFY hideLibrariesChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY totalCountChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        ItemIdRole = Qt::UserRole + 1,
        NameRole,
        SummaryRole,
        DescriptionRole,
        CategoryRole,
        VersionRole,
        InstalledVersionRole,
        StatusRole,
        SourceRole,
        RepoRole,
        RefRole,
        IconNameRole,
        IconPathRole,
        IconUrlRole,
        ScreenshotRole,
        HomepageRole,
        LicenseRole,
        SizeTextRole,
        TargetsRole,
        InstalledRole,
        UpgradableRole,
        RuntimeRole,
        SelectedRole,
        DownloadSizeRole,
        InstalledSizeRole,
    };
    Q_ENUM(Role)

    explicit CatalogModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString search() const { return m_search; }
    QString statusFilter() const { return m_statusFilter; }
    bool hideLibraries() const { return m_hideLibraries; }
    int selectedCount() const;
    int totalCount() const { return m_items.size(); }
    int count() const { return m_visible.size(); }

    void setItems(QVector<CatalogItem> items);
    QVector<CatalogItem> selectedItems() const;
    const CatalogItem* itemAt(int row) const;

    Q_INVOKABLE void toggle(int row);
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void selectAllVisible();

public slots:
    void setSearch(const QString& value);
    void setStatusFilter(const QString& value);
    void setHideLibraries(bool value);

signals:
    void searchChanged();
    void statusFilterChanged();
    void hideLibrariesChanged();
    void selectionChanged();
    void totalCountChanged();
    void countChanged();

private:
    void rebuildVisible();
    static bool isLibraryOrDevelopmentPackage(const QString& name);
    bool matches(const CatalogItem& item) const;

    QVector<CatalogItem> m_items;
    QVector<int> m_visible;
    QString m_search;
    QString m_statusFilter = QStringLiteral("all");
    bool m_hideLibraries = true;
};
