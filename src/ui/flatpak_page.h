#pragma once

#include "flatpak.h"
#include "flatpak_model.h"

#include <QComboBox>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableView;

/// Flatpak page: scope (system/user) and remote selection, app/runtime
/// discovery with installed-state detection, filters, batch install /
/// uninstall, update-all, remove-unused and remote management.
class FlatpakPage : public QWidget {
    Q_OBJECT

public:
    explicit FlatpakPage(QWidget* parent = nullptr);

    void setBackend(FlatpakBackend* backend) { m_backend = backend; }

    /// Populate the table from backend listing results (async caller).
    void setRefs(const QList<FlatpakBackend::Ref>& refs, const QString& remoteName);
    void setRemotes(const QStringList& remotes);
    void setCounts(int totalApps, int totalInstalled, const QString& totalSize);
    void setBusy(bool busy);
    void setWarningShown();

    /// True once a listing has been delivered (used by the UI test harness).
    bool countsVisible() const { return m_listingDelivered; }

    QString currentRemote() const { return m_remoteCombo->currentText(); }
    bool userScope() const { return m_scopeCombo->currentIndex() == 1; }
    QStringList checkedAppIds() const { return m_model->checkedAppIds(); }
    int checkedCount() const { return m_model->checkedCount(); }
    bool selectionInstalled() const { return m_model->allCheckedInstalled(); }
    bool selectionNotInstalled() const { return m_model->allCheckedNotInstalled(); }

    bool warningShown() const { return m_warningShown; }
    void markWarningShown() { m_warningShown = true; }

signals:
    void scopeChanged(bool userScope);
    void remoteChanged(const QString& remote);
    void filterChanged();
    void installRequested(const QStringList& appIds);
    void uninstallRequested(const QStringList& appIds);
    void updateAllRequested();
    void removeUnusedRequested();
    void manageRemotesRequested();
    void infoRequested(const QString& appId);

private:
    void updateActionButtons();
    void refreshCounts();

    FlatpakBackend* m_backend = nullptr;
    FlatpakModel* m_model = nullptr;
    FlatpakFilterProxy* m_proxy = nullptr;
    QTableView* m_table = nullptr;
    QComboBox* m_scopeCombo = nullptr;
    QComboBox* m_remoteCombo = nullptr;
    QComboBox* m_filterCombo = nullptr;
    QLineEdit* m_search = nullptr;
    QLabel* m_countApps = nullptr;
    QLabel* m_countInst = nullptr;
    QLabel* m_countSize = nullptr;
    QPushButton* m_manageRemotes = nullptr;
    QPushButton* m_updateAll = nullptr;
    QPushButton* m_removeUnused = nullptr;
    QPushButton* m_install = nullptr;
    QPushButton* m_uninstall = nullptr;
    QLabel* m_selection = nullptr;
    bool m_warningShown = false;
    bool m_listingDelivered = false;
};
