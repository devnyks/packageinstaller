#include "package_card.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

PackageCard::PackageCard(QWidget* parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("card"));
    setAttribute(Qt::WA_StyledBackground, true);
    setCursor(Qt::PointingHandCursor);
    setFixedHeight(96);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(14, 12, 14, 12);
    root->setSpacing(12);

    m_icon = new QLabel(this);
    m_icon->setFixedSize(56, 56);
    m_icon->setAlignment(Qt::AlignCenter);
    root->addWidget(m_icon, 0, Qt::AlignTop);

    auto* mid = new QVBoxLayout();
    mid->setSpacing(2);
    m_nameLabel = new QLabel(this);
    m_nameLabel->setObjectName(QStringLiteral("cardTitle"));
    m_nameLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    mid->addWidget(m_nameLabel);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setObjectName(QStringLiteral("pageSubtitle"));
    m_summaryLabel->setWordWrap(true);
    m_summaryLabel->setMaximumHeight(34);
    mid->addWidget(m_summaryLabel);

    auto* meta = new QHBoxLayout();
    meta->setSpacing(8);
    m_versionLabel = new QLabel(this);
    m_versionLabel->setObjectName(QStringLiteral("pageSubtitle"));
    meta->addWidget(m_versionLabel);
    m_badge = new QLabel(this);
    m_badge->setObjectName(QStringLiteral("cardBadge"));
    meta->addWidget(m_badge);
    meta->addStretch(1);
    mid->addLayout(meta);
    root->addLayout(mid, 1);

    auto* right = new QVBoxLayout();
    right->setSpacing(8);
    m_check = new QCheckBox(tr("Select"), this);
    right->addWidget(m_check, 0, Qt::AlignRight);
    m_action = new QPushButton(this);
    m_action->setFixedHeight(30);
    m_action->setCursor(Qt::PointingHandCursor);
    right->addWidget(m_action, 0, Qt::AlignRight);
    right->addStretch(1);
    root->addLayout(right);

    connect(m_check, &QCheckBox::toggled, this, [this](bool checked) {
        emit checkChanged(m_name, checked);
    });
    connect(m_action, &QPushButton::clicked, this, [this] {
        emit actionRequested(m_name);
    });
}

void PackageCard::setPackage(const QString& name, const QString& summary, const QString& version, State state) {
    m_name = name;
    m_state = state;
    m_nameLabel->setText(name);
    m_summaryLabel->setText(summary);
    m_versionLabel->setText(version);
    m_badge->setText(state == State::Upgradable ? tr("Update available") : (state == State::Installed ? tr("Installed") : QString()));
    m_badge->setProperty("status", state == State::Upgradable ? QStringLiteral("upgradable")
            : (state == State::Installed ? QStringLiteral("installed") : QStringLiteral("notInstalled")));
    m_badge->style()->unpolish(m_badge);
    m_badge->style()->polish(m_badge);
    updateActionButton();
}

void PackageCard::setIcon(const QIcon& icon) {
    m_icon->setPixmap(icon.pixmap(48, 48));
}

void PackageCard::setDescription(const QString& description) {
    m_description = description;
}

bool PackageCard::isChecked() const {
    return m_check->isChecked();
}

void PackageCard::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && rect().contains(event->pos())) {
        emit clicked(m_name);
    }
    QWidget::mouseReleaseEvent(event);
}

void PackageCard::enterEvent(QEnterEvent* event) {
    m_hover = true;
    setProperty("cardHover", true);
    style()->unpolish(this);
    style()->polish(this);
    QWidget::enterEvent(event);
}

void PackageCard::leaveEvent(QEvent* event) {
    m_hover = false;
    setProperty("cardHover", false);
    style()->unpolish(this);
    style()->polish(this);
    QWidget::leaveEvent(event);
}

void PackageCard::updateActionButton() {
    switch (m_state) {
    case State::NotInstalled:
        m_action->setText(tr("Install"));
        m_action->setProperty("accent", true);
        m_action->setProperty("danger", false);
        break;
    case State::Installed:
        m_action->setText(tr("Remove"));
        m_action->setProperty("accent", false);
        m_action->setProperty("danger", true);
        break;
    case State::Upgradable:
        m_action->setText(tr("Update"));
        m_action->setProperty("accent", true);
        m_action->setProperty("danger", false);
        break;
    }
    m_action->style()->unpolish(m_action);
    m_action->style()->polish(m_action);
}
