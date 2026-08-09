#include "progress_overlay.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

ProgressOverlay::ProgressOverlay(const QString& label, QWidget* parent)
    : QDialog(parent) {
    setWindowModality(Qt::WindowModal);
    setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint
        | Qt::WindowSystemMenuHint | Qt::WindowStaysOnTopHint);
    setWindowTitle(tr("Please wait..."));
    setFixedWidth(360);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 18);
    root->setSpacing(12);

    auto* text = new QLabel(label, this);
    text->setWordWrap(true);
    root->addWidget(text);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("pageSubtitle"));
    m_status->setWordWrap(true);
    m_status->hide();
    root->addWidget(m_status);

    auto* bar = new QProgressBar(this);
    bar->setRange(0, 0);  // busy indicator
    bar->setTextVisible(false);
    bar->setFixedHeight(6);
    root->addWidget(bar);

    auto* row = new QHBoxLayout();
    row->addStretch(1);
    m_cancel = new QPushButton(tr("Cancel"), this);
    m_cancel->setEnabled(false);
    row->addWidget(m_cancel);
    root->addLayout(row);

    connect(m_cancel, &QPushButton::clicked, this, &ProgressOverlay::cancelRequested);
}

void ProgressOverlay::setBusy(bool busy) {
    m_cancel->setEnabled(busy);
}

void ProgressOverlay::setStatus(const QString& text) {
    m_status->setText(text);
    m_status->setVisible(!text.isEmpty());
}
