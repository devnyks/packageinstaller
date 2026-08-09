#pragma once

#include "package_model.h"

#include <QWidget>

class QComboBox;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableView;

/// Repository browsing page (reference "Repo" tab): full sync-db package
/// table with search, status filters, lib/dev filtering, batch install /
/// uninstall, full-system upgrade and orphan removal.
class RepoPage : public QWidget {
    Q_OBJECT

public:
    explicit RepoPage(QWidget* parent = nullptr);

    void setPackageRows(std::vector<PackageRow> rows);
    void setOrphansExist(bool exist);

    void resetFilters();
    QStringList checkedNames() const { return m_model->checkedNames(); }
    int checkedCount() const { return m_model->checkedCount(); }
    int packageCount() const { return m_model->rowCount(); }
    int filteredRowCount() const { return m_proxy->rowCount(); }
    int upgradableCount() const { return m_model->upgradableCount(); }
    void setSearchFilter(const QString& text) { m_proxy->setSearchText(text); }
    void setStatusFilterIndex(int idx) {
        m_proxy->setStatusFilter(static_cast<PackageFilterProxy::StatusFilter>(idx));
        m_model->clearChecks();
    }

    /// Whether the current selection is entirely upgradable (Install->Upgrade).
    bool selectionAllUpgradable() const;
    bool selectionAllInstalled() const;

signals:
    void installRequested(const QStringList& names);
    void uninstallRequested(const QStringList& names);
    void upgradeAllRequested();
    void removeOrphansRequested();
    void infoRequested(const QString& name);

private:
    void updateActionButtons();
    void refreshCounts();

    PackageModel* m_model = nullptr;
    PackageFilterProxy* m_proxy = nullptr;
    QTableView* m_table = nullptr;
    QLineEdit* m_search = nullptr;
    QComboBox* m_statusFilter = nullptr;
    QCheckBox* m_hideLibs = nullptr;
    QLabel* m_countApps = nullptr;
    QLabel* m_countUpgr = nullptr;
    QLabel* m_countInst = nullptr;
    QPushButton* m_upgradeAll = nullptr;
    QPushButton* m_removeOrphans = nullptr;
    QPushButton* m_install = nullptr;
    QPushButton* m_uninstall = nullptr;
    QLabel* m_selection = nullptr;
};
