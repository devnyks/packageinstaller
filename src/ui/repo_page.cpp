#include "repo_page.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

RepoPage::RepoPage(QWidget* parent)
    : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(14);

    // header
    auto* header = new QHBoxLayout();
    header->setSpacing(12);
    auto* titleCol = new QVBoxLayout();
    titleCol->setSpacing(2);
    auto* title = new QLabel(tr("Packages"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    titleCol->addWidget(title);
    auto* subtitle = new QLabel(tr("All packages from the configured repositories"), this);
    subtitle->setObjectName(QStringLiteral("pageSubtitle"));
    titleCol->addWidget(subtitle);
    header->addLayout(titleCol);
    header->addStretch(1);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Search packages..."));
    m_search->setClearButtonEnabled(true);
    m_search->setMinimumWidth(240);
    header->addWidget(m_search);

    m_statusFilter = new QComboBox(this);
    m_statusFilter->addItem(tr("All packages"));
    m_statusFilter->addItem(tr("Upgradable"));
    m_statusFilter->addItem(tr("Installed"));
    m_statusFilter->addItem(tr("Not installed"));
    header->addWidget(m_statusFilter);

    m_hideLibs = new QCheckBox(tr("Hide library and developer packages"), this);
    header->addWidget(m_hideLibs);
    root->addLayout(header);

    // stats row
    auto* stats = new QHBoxLayout();
    stats->setSpacing(18);
    m_countApps = new QLabel(this);
    m_countApps->setObjectName(QStringLiteral("pageSubtitle"));
    stats->addWidget(m_countApps);
    m_countUpgr = new QLabel(this);
    m_countUpgr->setObjectName(QStringLiteral("pageSubtitle"));
    stats->addWidget(m_countUpgr);
    m_countInst = new QLabel(this);
    m_countInst->setObjectName(QStringLiteral("pageSubtitle"));
    stats->addWidget(m_countInst);
    stats->addStretch(1);
    m_upgradeAll = new QPushButton(tr("Upgrade all"), this);
    m_upgradeAll->setProperty("accent", true);
    stats->addWidget(m_upgradeAll);
    m_removeOrphans = new QPushButton(tr("Remove orphaned packages"), this);
    stats->addWidget(m_removeOrphans);
    root->addLayout(stats);

    // table
    m_model = new PackageModel(this);
    m_proxy = new PackageFilterProxy(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->sort(PackageModel::Name, Qt::AscendingOrder);

    m_table = new QTableView(this);
    m_table->setModel(m_proxy);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setSortingEnabled(true);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(PackageModel::Name, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(PackageModel::Version, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(PackageModel::Description, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(PackageModel::StatusCol, QHeaderView::ResizeToContents);
    m_table->verticalHeader()->hide();
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    root->addWidget(m_table, 1);

    // action bar
    auto* bar = new QHBoxLayout();
    m_selection = new QLabel(tr("Select packages to install"), this);
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

    // connections
    connect(m_search, &QLineEdit::textChanged, m_proxy, &PackageFilterProxy::setSearchText);
    connect(m_statusFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        m_proxy->setStatusFilter(static_cast<PackageFilterProxy::StatusFilter>(idx));
        m_model->clearChecks();
        updateActionButtons();
    });
    connect(m_hideLibs, &QCheckBox::toggled, m_proxy, &PackageFilterProxy::setHideLibs);
    connect(m_model, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex&, const QModelIndex&, const QList<int>&) {
        updateActionButtons();
    });
    connect(m_install, &QPushButton::clicked, this, [this] {
        if (const auto names = checkedNames(); !names.isEmpty()) {
            emit installRequested(names);
        }
    });
    connect(m_uninstall, &QPushButton::clicked, this, [this] {
        if (const auto names = checkedNames(); !names.isEmpty()) {
            emit uninstallRequested(names);
        }
    });
    connect(m_upgradeAll, &QPushButton::clicked, this, &RepoPage::upgradeAllRequested);
    connect(m_removeOrphans, &QPushButton::clicked, this, &RepoPage::removeOrphansRequested);
    connect(m_table, &QTableView::doubleClicked, this, [this](const QModelIndex& idx) {
        const QModelIndex src = m_proxy->mapToSource(idx);
        if (src.isValid()) {
            emit infoRequested(m_model->data(src, PackageModel::PackageNameRole).toString());
        }
    });

    updateActionButtons();
}

void RepoPage::setPackageRows(std::vector<PackageRow> rows) {
    m_model->setRows(std::move(rows));
    refreshCounts();
    updateActionButtons();
}

void RepoPage::setOrphansExist(bool exist) {
    m_removeOrphans->setVisible(exist);
}

void RepoPage::resetFilters() {
    m_search->clear();
    m_statusFilter->setCurrentIndex(0);
    m_hideLibs->setChecked(false);
    m_proxy->refresh();
}

bool RepoPage::selectionAllUpgradable() const {
    const QStringList names = m_model->checkedNames();
    if (names.isEmpty()) {
        return false;
    }
    for (const QString& n : names) {
        const auto idx = m_model->indexForName(n);
        if (!idx.isValid()) {
            return false;
        }
        if (m_model->data(idx, PackageModel::StateRole).toInt() != static_cast<int>(PackageRow::State::Upgradable)) {
            return false;
        }
    }
    return true;
}

bool RepoPage::selectionAllInstalled() const {
    const QStringList names = m_model->checkedNames();
    if (names.isEmpty()) {
        return false;
    }
    for (const QString& n : names) {
        const auto idx = m_model->indexForName(n);
        if (!idx.isValid()) {
            return false;
        }
        const int state = m_model->data(idx, PackageModel::StateRole).toInt();
        if (state == static_cast<int>(PackageRow::State::NotInstalled)) {
            return false;
        }
    }
    return true;
}

void RepoPage::updateActionButtons() {
    const int n = m_model->checkedCount();
    m_selection->setText(n > 0 ? tr("%1 selected").arg(n) : tr("Select packages to install"));
    m_install->setEnabled(n > 0);
    m_install->setText(selectionAllUpgradable() ? tr("Upgrade") : tr("Install"));
    m_uninstall->setEnabled(n > 0 && selectionAllInstalled());
    refreshCounts();
}

void RepoPage::refreshCounts() {
    const int total = m_model->rowCount();
    const int upgr = m_model->upgradableCount();
    const int inst = total - m_model->countOf( [](const PackageRow& r) {
        return r.state == PackageRow::State::NotInstalled;
    });
    m_countApps->setText(tr("Packages: %1").arg(total));
    m_countUpgr->setText(tr("Upgradable: %1").arg(upgr));
    m_countInst->setText(tr("Installed: %1").arg(inst));
    m_upgradeAll->setVisible(upgr > 0);
}
