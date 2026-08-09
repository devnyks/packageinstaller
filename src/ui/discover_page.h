#pragma once

#include "appstream_provider.h"
#include "package_card.h"
#include "package_detail.h"
#include "pkglist.h"

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QGridLayout;
class QVBoxLayout;

/// Discover page: curated applications from pkglist.yaml, grouped by
/// category. Category grid -> package card grid with AppStream artwork,
/// batch selection and a detail panel.
class DiscoverPage : public QWidget {
    Q_OBJECT

public:
    struct EntryView {
        PopularEntry entry;
        QString version;
        QString description;
        QStringList extras;
        PackageCard::State state = PackageCard::State::NotInstalled;
        QString iconKey;  // appstream id or package name for icon lookup
        int row = 0;      // current grid row (for re-layout)
        int col = 0;
        QWidget* card = nullptr;
    };

    explicit DiscoverPage(QWidget* parent = nullptr);

    /// Feed in the parsed curated list and package states. `installed` /
    /// `upgradable` are name sets; `versions`/`descriptions` per name.
    void setEntries(const QList<PopularEntry>& entries, const QSet<QString>& installed,
        const QSet<QString>& upgradable, const QHash<QString, QString>& versions,
        const QHash<QString, QString>& descriptions);
    void setAppStream(AppStreamProvider* provider);

    void clearSelection();
    void refresh();  // recompute states after operations

    QStringList selectedNames() const;
    bool hasSelection() const { return !m_selected.isEmpty(); }

signals:
    void installRequested(const QStringList& names);
    void uninstallRequested(const QStringList& names);
    void packageInfoRequested(const QString& name);

private:
    void rebuildCategoryGrid();
    void rebuildAppGrid();
    void showCategory(const QString& category);
    void showAll();
    void onCardClicked(const QString& name);
    void updateSelectionBar();
    void applySearch();
    QWidget* makeCategoryCard(const QString& category, int count);

    QList<PopularEntry> m_entries;
    QSet<QString> m_installed;
    QSet<QString> m_upgradable;
    QHash<QString, QString> m_versions;
    QHash<QString, QString> m_descriptions;
    AppStreamProvider* m_appstream = nullptr;
    bool m_appstreamReady = false;

    QHash<QString, QList<EntryView>> m_categoryViews;
    QString m_currentCategory;

    QLineEdit* m_search = nullptr;
    QLabel* m_countLabel = nullptr;
    QStackedWidget* m_stack = nullptr;
    QScrollArea* m_categoriesScroll = nullptr;
    QWidget* m_categoriesWidget = nullptr;
    QGridLayout* m_categoriesLayout = nullptr;
    QScrollArea* m_appsScroll = nullptr;
    QWidget* m_appsWidget = nullptr;
    QGridLayout* m_appsGrid = nullptr;
    PackageDetail* m_detail = nullptr;
    QWidget* m_gridArea = nullptr;  // container for grid + detail

    QLabel* m_selectionLabel = nullptr;
    QPushButton* m_installSelected = nullptr;
    QPushButton* m_uninstallSelected = nullptr;
    QPushButton* m_backButton = nullptr;
    QLabel* m_headerTitle = nullptr;

    QSet<QString> m_selected;
    QHash<QString, PackageCard*> m_cardByName;
    QHash<QString, AppStreamProvider::Component> m_appstreamComponents;
};
