#include "discover_page.h"

#include "logging.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStyle>
#include <QVBoxLayout>

#include <QMouseEvent>

namespace {

constexpr int kCardColumns = 3;
constexpr int kGridColumns = 2;

QIcon categoryIcon(const QString& category) {
    static const QHash<QString, QString> icons{
        {QStringLiteral("Audio"), QStringLiteral("audio-x-generic")},
        {QStringLiteral("Browsers"), QStringLiteral("applications-internet")},
        {QStringLiteral("Communication"), QStringLiteral("internet-telephony")},
        {QStringLiteral("Development"), QStringLiteral("applications-development")},
        {QStringLiteral("Games"), QStringLiteral("applications-games")},
        {QStringLiteral("Graphics"), QStringLiteral("applications-graphics")},
        {QStringLiteral("Hardware Tools"), QStringLiteral("computer")},
        {QStringLiteral("Internet"), QStringLiteral("applications-internet")},
        {QStringLiteral("Mail"), QStringLiteral("mail-message-new")},
        {QStringLiteral("Multimedia"), QStringLiteral("applications-multimedia")},
        {QStringLiteral("Office"), QStringLiteral("x-office-document")},
        {QStringLiteral("Other"), QStringLiteral("folder")},
        {QStringLiteral("Video"), QStringLiteral("applications-multimedia")},
        {QStringLiteral("Virtualization"), QStringLiteral("computer")},
    };
    return QIcon::fromTheme(icons.value(category, QStringLiteral("folder")),
        QIcon(QStringLiteral(":/icons/app-icon-16.png")));
}

}  // namespace

DiscoverPage::DiscoverPage(QWidget* parent)
    : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(14);

    // header
    auto* header = new QHBoxLayout();
    header->setSpacing(12);
    m_backButton = new QPushButton(this);
    m_backButton->setIcon(QIcon::fromTheme(QStringLiteral("go-previous")));
    m_backButton->setToolTip(tr("Back to categories"));
    m_backButton->hide();
    header->addWidget(m_backButton);

    auto* titleCol = new QVBoxLayout();
    titleCol->setSpacing(2);
    m_headerTitle = new QLabel(tr("Discover"), this);
    m_headerTitle->setObjectName(QStringLiteral("pageTitle"));
    titleCol->addWidget(m_headerTitle);
    auto* subtitle = new QLabel(tr("Curated applications for CachyOS"), this);
    subtitle->setObjectName(QStringLiteral("pageSubtitle"));
    titleCol->addWidget(subtitle);
    header->addLayout(titleCol);
    header->addStretch(1);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Search applications..."));
    m_search->setClearButtonEnabled(true);
    m_search->setMinimumWidth(260);
    header->addWidget(m_search);
    root->addLayout(header);

    // category count label
    m_countLabel = new QLabel(this);
    m_countLabel->setObjectName(QStringLiteral("pageSubtitle"));
    root->addWidget(m_countLabel);

    // main content: categories / apps
    m_stack = new QStackedWidget(this);

    m_categoriesScroll = new QScrollArea(this);
    m_categoriesScroll->setWidgetResizable(true);
    m_categoriesWidget = new QWidget(this);
    m_categoriesLayout = new QGridLayout(m_categoriesWidget);
    m_categoriesLayout->setContentsMargins(0, 0, 0, 0);
    m_categoriesLayout->setSpacing(12);
    m_categoriesScroll->setWidget(m_categoriesWidget);
    m_stack->addWidget(m_categoriesScroll);

    // apps view: grid + detail panel
    m_gridArea = new QWidget(this);
    auto* gridRoot = new QHBoxLayout(m_gridArea);
    gridRoot->setContentsMargins(0, 0, 0, 0);
    gridRoot->setSpacing(16);

    m_appsScroll = new QScrollArea(m_gridArea);
    m_appsScroll->setWidgetResizable(true);
    m_appsWidget = new QWidget(m_gridArea);
    m_appsGrid = new QGridLayout(m_appsWidget);
    m_appsGrid->setContentsMargins(0, 0, 0, 0);
    m_appsGrid->setSpacing(12);
    m_appsScroll->setWidget(m_appsWidget);
    gridRoot->addWidget(m_appsScroll, 1);

    m_detail = new PackageDetail(m_gridArea);
    m_detail->hide();
    gridRoot->addWidget(m_detail);
    m_stack->addWidget(m_gridArea);

    root->addWidget(m_stack, 1);

    // selection bar
    auto* selBar = new QHBoxLayout();
    m_selectionLabel = new QLabel(this);
    m_selectionLabel->setObjectName(QStringLiteral("pageSubtitle"));
    selBar->addWidget(m_selectionLabel);
    selBar->addStretch(1);
    m_uninstallSelected = new QPushButton(tr("Uninstall selected"), this);
    m_uninstallSelected->setProperty("danger", true);
    selBar->addWidget(m_uninstallSelected);
    m_installSelected = new QPushButton(tr("Install selected"), this);
    m_installSelected->setProperty("accent", true);
    selBar->addWidget(m_installSelected);
    root->addLayout(selBar);

    connect(m_search, &QLineEdit::textChanged, this, [this](const QString&) { applySearch(); });
    connect(m_backButton, &QPushButton::clicked, this, &DiscoverPage::showAll);
    connect(m_installSelected, &QPushButton::clicked, this, [this] {
        emit installRequested(m_selected.values());
    });
    connect(m_uninstallSelected, &QPushButton::clicked, this, [this] {
        emit uninstallRequested(m_selected.values());
    });
    connect(m_detail, &PackageDetail::actionRequested, this, [this](const QString& name, int action) {
        if (action == 0) {
            emit installRequested({name});
        } else {
            emit uninstallRequested({name});
        }
    });

    updateSelectionBar();
}

void DiscoverPage::setAppStream(AppStreamProvider* provider) {
    m_appstream = provider;
    connect(provider, &AppStreamProvider::ready, this, [this] {
        m_appstreamReady = true;
        refresh();
    });
}

void DiscoverPage::setEntries(const QList<PopularEntry>& entries, const QSet<QString>& installed,
    const QSet<QString>& upgradable, const QHash<QString, QString>& versions,
    const QHash<QString, QString>& descriptions) {
    m_entries = entries;
    m_installed = installed;
    m_upgradable = upgradable;
    m_versions = versions;
    m_descriptions = descriptions;
    m_selected.clear();
    for (auto* card : std::as_const(m_cardByName)) {
        card->deleteLater();
    }
    m_cardByName.clear();
    m_categoryViews.clear();
    rebuildCategoryGrid();
    rebuildAppGrid();
}

void DiscoverPage::refresh() {
    if (m_entries.isEmpty()) {
        return;
    }
    for (auto it = m_categoryViews.begin(); it != m_categoryViews.end(); ++it) {
        for (auto& view : it.value()) {
            const bool anyNotInstalled = std::any_of(view.entry.installNames.begin(), view.entry.installNames.end(),
                [this](const QString& n) { return !m_installed.contains(n); });
            const bool anyUpgradable = std::any_of(view.entry.installNames.begin(), view.entry.installNames.end(),
                [this](const QString& n) { return m_upgradable.contains(n); });
            view.state = anyUpgradable ? PackageCard::State::Upgradable
                : (anyNotInstalled ? PackageCard::State::NotInstalled : PackageCard::State::Installed);
        }
    }
    rebuildAppGrid();
}

void DiscoverPage::rebuildCategoryGrid() {
    // clear existing category cards (children of the layout)
    while (QLayoutItem* item = m_categoriesLayout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    QHash<QString, int> counts;
    for (const auto& entry : m_entries) {
        counts[entry.group] += 1;
    }

    int col = 0;
    int row = 0;
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it) {
        m_categoriesLayout->addWidget(makeCategoryCard(it.key(), it.value()), row, col);
        ++col;
        if (col >= kCardColumns) {
            col = 0;
            ++row;
        }
    }
    m_categoriesLayout->setColumnStretch(kCardColumns, 1);
    m_categoriesLayout->setRowStretch(row + 1, 1);
}

QWidget* DiscoverPage::makeCategoryCard(const QString& category, int count) {
    auto* card = new QWidget(m_categoriesWidget);
    card->setObjectName(QStringLiteral("categoryCard"));
    card->setAttribute(Qt::WA_StyledBackground, true);
    card->setCursor(Qt::PointingHandCursor);
    card->setMinimumHeight(72);

    auto* lay = new QHBoxLayout(card);
    lay->setContentsMargins(16, 12, 16, 12);
    lay->setSpacing(14);
    auto* icon = new QLabel(card);
    icon->setPixmap(categoryIcon(category).pixmap(40, 40));
    lay->addWidget(icon);
    auto* textCol = new QVBoxLayout();
    auto* name = new QLabel(category, card);
    name->setObjectName(QStringLiteral("cardTitle"));
    textCol->addWidget(name);
    auto* sub = new QLabel(tr("%1 applications").arg(count), card);
    sub->setObjectName(QStringLiteral("pageSubtitle"));
    textCol->addWidget(sub);
    lay->addLayout(textCol);
    lay->addStretch(1);

    card->setMouseTracking(true);
    QObject::connect(card, &QWidget::destroyed, this, [category] { Q_UNUSED(category); });

    // hover + click via event filter-less approach: install an event handler
    class CardEventFilter : public QObject {
    public:
        CardEventFilter(QWidget* w, QString cat, DiscoverPage* page, QObject* parent)
            : QObject(parent), m_w(w), m_cat(std::move(cat)), m_page(page) {}
        bool eventFilter(QObject* obj, QEvent* ev) override {
            if (obj == m_w) {
                if (ev->type() == QEvent::Enter) {
                    m_w->setProperty("cardHover", true);
                    m_w->style()->unpolish(m_w);
                    m_w->style()->polish(m_w);
                } else if (ev->type() == QEvent::Leave) {
                    m_w->setProperty("cardHover", false);
                    m_w->style()->unpolish(m_w);
                    m_w->style()->polish(m_w);
                } else if (ev->type() == QEvent::MouseButtonRelease) {
                    m_page->showCategory(m_cat);
                }
            }
            return QObject::eventFilter(obj, ev);
        }
        QWidget* m_w;
        QString m_cat;
        DiscoverPage* m_page;
    };
    card->installEventFilter(new CardEventFilter(card, category, this, card));
    return card;
}

void DiscoverPage::rebuildAppGrid() {
    if (m_currentCategory.isEmpty() || m_entries.isEmpty()) {
        return;
    }

    // remove old cards
    while (QLayoutItem* item = m_appsGrid->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
    m_cardByName.clear();

    int col = 0;
    int row = 0;
    for (auto& view : m_categoryViews.value(m_currentCategory)) {
        auto* card = new PackageCard(m_appsWidget);
        const QString primary = view.entry.installNames.first();
        card->setPackage(primary, view.description, view.version, view.state);
        card->setProperty("summary", view.description);

        // AppStream artwork (best-effort; falls back to the theme icon)
        QIcon icon = QIcon::fromTheme(QStringLiteral("package-x-generic"),
            QIcon(QStringLiteral(":/icons/app-icon-64.png")));
        if (m_appstreamReady && m_appstream) {
            if (auto comp = m_appstream->byPackage(primary)) {
                view.iconKey = comp->iconName;
                if (!comp->iconFile.isEmpty()) {
                    icon = QIcon(comp->iconFile);
                } else if (!comp->iconName.isEmpty()) {
                    icon = QIcon::fromTheme(comp->iconName, icon);
                }
                card->setDescription(comp->summary);
            } else if (auto comp2 = m_appstream->byId(primary)) {
                if (!comp2->iconFile.isEmpty()) {
                    icon = QIcon(comp2->iconFile);
                } else if (!comp2->iconName.isEmpty()) {
                    icon = QIcon::fromTheme(comp2->iconName, icon);
                }
                card->setDescription(comp2->summary);
            }
        }
        card->setIcon(icon);

        view.card = card;
        m_cardByName.insert(primary, card);
        connect(card, &PackageCard::clicked, this, &DiscoverPage::onCardClicked);
        connect(card, &PackageCard::actionRequested, this, [this](const QString& name) {
            if (m_selected.isEmpty()) {
                emit installRequested({name});
            }
        });
        connect(card, &PackageCard::checkChanged, this, [this](const QString& name, bool checked) {
            if (checked) {
                m_selected.insert(name);
            } else {
                m_selected.remove(name);
            }
            updateSelectionBar();
        });

        m_appsGrid->addWidget(card, row, col);
        ++col;
        if (col >= kGridColumns) {
            col = 0;
            ++row;
        }
    }
    m_appsGrid->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding), row + 1, 0);
    m_appsGrid->setRowStretch(row + 1, 1);
    applySearch();
}

void DiscoverPage::showCategory(const QString& category) {
    m_currentCategory = category;
    m_headerTitle->setText(category);
    m_backButton->show();
    m_countLabel->setText(tr("%1 applications").arg(m_categoryViews.value(category).size()));
    m_stack->setCurrentWidget(m_gridArea);
    m_detail->hide();
    rebuildAppGrid();
}

void DiscoverPage::showAll() {
    m_currentCategory.clear();
    m_headerTitle->setText(tr("Discover"));
    m_backButton->hide();
    m_countLabel->setText(tr("%1 categories").arg(m_categoryViews.size()));
    m_stack->setCurrentWidget(m_categoriesScroll);
    m_detail->hide();
    updateSelectionBar();
    applySearch();
}

void DiscoverPage::onCardClicked(const QString& name) {
    for (auto it = m_categoryViews.begin(); it != m_categoryViews.end(); ++it) {
        for (auto& view : it.value()) {
            if (view.entry.installNames.first() != name) {
                continue;
            }
            QStringList extras;
            extras << tr("Packages to install: %1").arg(view.entry.installNames.join(QLatin1Char(' ')));
            const AppStreamProvider::Component* comp = nullptr;
            if (m_appstreamReady && m_appstream) {
                if (auto c = m_appstream->byPackage(name)) {
                    m_appstreamComponents.insert(name, *c);
                    comp = &m_appstreamComponents[name];
                }
            }
            const auto detailState = view.state == PackageCard::State::Upgradable
                ? PackageDetail::State::Upgradable
                : (view.state == PackageCard::State::Installed ? PackageDetail::State::Installed
                                                               : PackageDetail::State::NotInstalled);
            m_detail->showPackage(name, view.version, view.description, extras, comp, detailState);
            m_detail->show();
            return;
        }
    }
}

void DiscoverPage::updateSelectionBar() {
    const int n = m_selected.size();
    m_selectionLabel->setText(n > 0 ? tr("%1 selected").arg(n) : tr("Select applications to batch-install"));
    m_installSelected->setEnabled(n > 0);
    m_uninstallSelected->setEnabled(n > 0);
}

void DiscoverPage::applySearch() {
    const QString text = m_search->text().trimmed();
    if (m_stack->currentWidget() == m_categoriesScroll) {
        for (int i = 0; i < m_categoriesLayout->count(); ++i) {
            QLayoutItem* item = m_categoriesLayout->itemAt(i);
            if (item->widget()) {
                const QString label = item->widget()->findChild<QLabel*>(QStringLiteral("cardTitle"))->text();
                item->widget()->setVisible(text.isEmpty() || label.contains(text, Qt::CaseInsensitive));
            }
        }
    } else {
        for (auto* card : std::as_const(m_cardByName)) {
            const QString summary = card->property("summary").toString();
            const bool match = card->packageName().contains(text, Qt::CaseInsensitive)
                || summary.contains(text, Qt::CaseInsensitive);
            card->setVisible(text.isEmpty() || match);
        }
    }
}

void DiscoverPage::clearSelection() {
    m_selected.clear();
    for (auto* card : std::as_const(m_cardByName)) {
        if (card->isChecked()) {
            card->setChecked(false);
        }
    }
    updateSelectionBar();
}

QStringList DiscoverPage::selectedNames() const {
    return m_selected.values();
}
