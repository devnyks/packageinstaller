#include "navbar.h"

#include <QApplication>
#include <QStyle>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QStyle>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
constexpr int kNavWidth = 176;
}

NavBar::NavBar(QWidget* parent)
    : QWidget(parent) {
    setFixedWidth(kNavWidth);
    setObjectName(QStringLiteral("navBar"));

    m_root = new QVBoxLayout(this);
    m_root->setContentsMargins(10, 14, 10, 14);
    m_root->setSpacing(4);

    auto* header = new QLabel(this);
    header->setPixmap(QIcon(QStringLiteral(":/icons/app-icon-64.png")).pixmap(38, 38));
    header->setAlignment(Qt::AlignHCenter);
    m_root->addWidget(header);

    auto* title = new QLabel(tr("Package\nInstaller"), this);
    title->setObjectName(QStringLiteral("navTitle"));
    title->setAlignment(Qt::AlignHCenter);
    title->setWordWrap(true);
    m_root->addWidget(title);
    m_root->addSpacing(12);

    m_group = new QButtonGroup(this);
    m_group->setExclusive(true);
    connect(m_group, &QButtonGroup::idClicked, this, [this](int id) {
        setCurrentIndex(id);
        emit itemActivated(id);
    });

    // stretch keeps the top items stacked and pins bottom items to the
    // bottom edge; normal items are inserted before it.
    m_root->addStretch(1);
}

int NavBar::addItem(const QString& iconName, const QString& text, bool bottom) {
    const int id = m_items.size();
    auto* btn = new QPushButton(this);
    btn->setCheckable(true);
    btn->setFixedHeight(40);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setProperty("navItem", true);

    auto* lay = new QHBoxLayout(btn);
    lay->setContentsMargins(12, 0, 12, 0);
    lay->setSpacing(10);
    auto* icon = new QLabel(btn);
    icon->setPixmap(QIcon::fromTheme(iconName).pixmap(20, 20));
    lay->addWidget(icon);
    auto* label = new QLabel(text, btn);
    lay->addWidget(label);
    lay->addStretch(1);

    m_group->addButton(btn, id);
    m_items.append(btn);

    if (bottom) {
        // pin below the stretch (at the bottom edge)
        m_root->removeItem(m_root->itemAt(m_root->count() - 1));
        m_root->addWidget(btn);
        m_root->addStretch(1);
    } else {
        // insert before the trailing stretch so items stay stacked at top
        m_root->insertWidget(m_root->count() - 1, btn);
    }
    if (id == 0) {
        btn->setChecked(true);
    }
    return id;
}

void NavBar::setCurrentIndex(int index) {
    if (index < 0 || index >= m_items.size()) {
        return;
    }
    auto* btn = m_items.at(index);
    btn->setChecked(true);
    btn->style()->unpolish(btn);
    btn->style()->polish(btn);
}

int NavBar::currentIndex() const {
    return m_group->checkedId();
}

void NavBar::setItemEnabled(int index, bool enabled) {
    if (index >= 0 && index < m_items.size()) {
        m_items.at(index)->setEnabled(enabled);
    }
}
