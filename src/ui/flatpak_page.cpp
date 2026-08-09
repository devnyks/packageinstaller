#include "flatpak_page.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

FlatpakPage::FlatpakPage(QWidget* parent)
    : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(14);

    auto* header = new QHBoxLayout();
    header->setSpacing(12);
    auto* titleCol = new QVBoxLayout();
    titleCol->setSpacing(2);
    auto* title = new QLabel(tr("Flatpak"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    titleCol->addWidget(title);
    auto* subtitle = new QLabel(tr("Applications and runtimes from Flatpak remotes"), this);
    subtitle->setObjectName(QStringLiteral("pageSubtitle"));
    titleCol->addWidget(subtitle);
    header->addLayout(titleCol);
    header->addStretch(1);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Search flatpaks..."));
    m_search->setClearButtonEnabled(true);
    m_search->setMinimumWidth(220);
    header->addWidget(m_search);

    m_filterCombo = new QComboBox(this);
    m_filterCombo->addItem(tr("All available"));
    m_filterCombo->addItem(tr("All installed"));
    m_filterCombo->addItem(tr("All apps"));
    m_filterCombo->addItem(tr("All runtimes"));
    m_filterCombo->addItem(tr("Installed apps"));
    m_filterCombo->addItem(tr("Installed runtimes"));
    m_filterCombo->addItem(tr("Not installed"));
    header->addWidget(m_filterCombo);

    m_scopeCombo = new QComboBox(this);
    m_scopeCombo->addItem(tr("For all users"));
    m_scopeCombo->addItem(tr("For current user"));
    header->addWidget(m_scopeCombo);

    m_remoteCombo = new QComboBox(this);
    m_remoteCombo->setMinimumWidth(160);
    header->addWidget(m_remoteCombo);

    m_manageRemotes = new QPushButton(tr("Manage remotes"), this);
    header->addWidget(m_manageRemotes);
    root->addLayout(header);

    auto* stats = new QHBoxLayout();
    stats->setSpacing(18);
    m_countApps = new QLabel(this);
    m_countApps->setObjectName(QStringLiteral("pageSubtitle"));
    stats->addWidget(m_countApps);
    m_countInst = new QLabel(this);
    m_countInst->setObjectName(QStringLiteral("pageSubtitle"));
    stats->addWidget(m_countInst);
    m_countSize = new QLabel(this);
    m_countSize->setObjectName(QStringLiteral("pageSubtitle"));
    stats->addWidget(m_countSize);
    stats->addStretch(1);
    m_updateAll = new QPushButton(tr("Update all"), this);
    m_updateAll->setProperty("accent", true);
    stats->addWidget(m_updateAll);
    m_removeUnused = new QPushButton(tr("Remove unused"), this);
    stats->addWidget(m_removeUnused);
    root->addLayout(stats);

    m_model = new FlatpakModel(this);
    m_proxy = new FlatpakFilterProxy(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->sort(FlatpakModel::Name, Qt::AscendingOrder);

    m_table = new QTableView(this);
    m_table->setModel(m_proxy);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setSortingEnabled(true);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(FlatpakModel::Name, QHeaderView::Stretch);
    m_table->verticalHeader()->hide();
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    root->addWidget(m_table, 1);

    auto* bar = new QHBoxLayout();
    m_selection = new QLabel(tr("Select flatpaks to install"), this);
    m_selection->setObjectName(QStringLiteral("pageSubtitle"));
    bar->addWidget(m_selection);
    bar->addStretch(1);
    m_uninstall = new QPushButton(tr("Uninstall"), this);
    m_uninstall->setProperty("danger", true);
    bar->addWidget(m_uninstall);
    m_install = new QPushButton(tr("Install"), this);
    m_install->setProperty("accent", true);
    bar->addWidget(m_install);
    root->addLayout(bar);

    connect(m_search, &QLineEdit::textChanged, m_proxy, &FlatpakFilterProxy::setSearchText);
    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        m_proxy->setFilter(static_cast<FlatpakFilterProxy::Filter>(idx));
        m_model->clearChecks();
        updateActionButtons();
    });
    connect(m_scopeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (m_backend) {
            m_backend->setUserScope(idx == 1);
        }
        emit scopeChanged(idx == 1);
    });
    connect(m_remoteCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        emit remoteChanged(m_remoteCombo->currentText());
    });
    connect(m_model, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex&, const QModelIndex&, const QList<int>&) {
        updateActionButtons();
    });
    connect(m_install, &QPushButton::clicked, this, [this] {
        if (const auto ids = checkedAppIds(); !ids.isEmpty()) {
            emit installRequested(ids);
        }
    });
    connect(m_uninstall, &QPushButton::clicked, this, [this] {
        if (const auto ids = checkedAppIds(); !ids.isEmpty()) {
            emit uninstallRequested(ids);
        }
    });
    connect(m_updateAll, &QPushButton::clicked, this, &FlatpakPage::updateAllRequested);
    connect(m_removeUnused, &QPushButton::clicked, this, &FlatpakPage::removeUnusedRequested);
    connect(m_manageRemotes, &QPushButton::clicked, this, &FlatpakPage::manageRemotesRequested);
    connect(m_table, &QTableView::doubleClicked, this, [this](const QModelIndex& idx) {
        const QModelIndex src = m_proxy->mapToSource(idx);
        if (src.isValid()) {
            emit infoRequested(m_model->data(src, FlatpakModel::AppIdRole).toString());
        }
    });

    updateActionButtons();
}

void FlatpakPage::setRefs(const QList<FlatpakBackend::Ref>& refs, const QString& remoteName) {
    m_listingDelivered = true;
    m_model->setRefs(refs);
    m_remoteCombo->blockSignals(true);
    if (m_remoteCombo->findText(remoteName) >= 0) {
        m_remoteCombo->setCurrentText(remoteName);
    }
    m_remoteCombo->blockSignals(false);
    m_filterCombo->setCurrentIndex(0);
    m_proxy->setFilter(FlatpakFilterProxy::Filter::AllAvailable);
    refreshCounts();
    updateActionButtons();
}

void FlatpakPage::setRemotes(const QStringList& remotes) {
    m_remoteCombo->blockSignals(true);
    const QString previous = m_remoteCombo->currentText();
    m_remoteCombo->clear();
    m_remoteCombo->addItems(remotes);
    if (remotes.contains(previous)) {
        m_remoteCombo->setCurrentText(previous);
    }
    m_remoteCombo->blockSignals(false);
}

void FlatpakPage::setCounts(int totalApps, int totalInstalled, const QString& totalSize) {
    m_countApps->setText(tr("Available: %1").arg(totalApps));
    m_countInst->setText(tr("Installed: %1").arg(totalInstalled));
    m_countSize->setText(tr("Total size: %1").arg(totalSize));
}

void FlatpakPage::setBusy(bool busy) {
    setEnabled(!busy);
    setCursor(busy ? QCursor(Qt::BusyCursor) : QCursor(Qt::ArrowCursor));
}

void FlatpakPage::setWarningShown() {
    m_warningShown = true;
}

void FlatpakPage::updateActionButtons() {
    const int n = m_model->checkedCount();
    m_selection->setText(n > 0 ? tr("%1 selected").arg(n) : tr("Select flatpaks to install"));
    m_install->setEnabled(n > 0 && m_model->allCheckedNotInstalled());
    m_uninstall->setEnabled(n > 0 && m_model->allCheckedInstalled());
}

void FlatpakPage::refreshCounts() {
    int totalApps = 0;
    int totalInstalled = 0;
    for (const auto& ref : m_model->refs()) {
        if (!ref.isRuntime) {
            ++totalApps;
        }
        if (ref.installed) {
            ++totalInstalled;
        }
    }
    m_countApps->setText(tr("Available: %1").arg(totalApps));
    m_countInst->setText(tr("Installed: %1").arg(totalInstalled));
}
