#include "confirm_dialog.h"

#include <QDialogButtonBox>
#include <QApplication>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

ConfirmDialog::ConfirmDialog(Mode mode, const QString& packageSummary,
    const QString& statusText, const QString& detailsText, bool hasConflicts, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(mode == Mode::Install ? tr("Confirm installation") : tr("Confirm removal"));
    setMinimumWidth(560);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 18);
    root->setSpacing(12);

    const QString heading = mode == Mode::Install
        ? tr("The following packages were selected. Click Show Details for the list of changes.")
        : tr("The following packages were selected for removal. Click Show Details for the list of changes.");
    auto* headingLabel = new QLabel(QStringLiteral("<b>%1</b>").arg(heading), this);
    headingLabel->setWordWrap(true);
    root->addWidget(headingLabel);

    auto* namesLabel = new QLabel(packageSummary, this);
    namesLabel->setWordWrap(true);
    root->addWidget(namesLabel);

    if (!statusText.isEmpty()) {
        auto* status = new QLabel(statusText.trimmed(), this);
        status->setObjectName(QStringLiteral("pageSubtitle"));
        root->addWidget(status);
    }

    if (hasConflicts) {
        auto* warn = new QLabel(tr("<b>The following packages have conflicts.</b>"), this);
        warn->setStyleSheet(QStringLiteral("color: %1;")
                                .arg(QApplication::palette().color(QPalette::Highlight).name()));
        root->addWidget(warn);
    }

    auto* details = new QTextEdit(this);
    details->setPlainText(detailsText.trimmed());
    details->setReadOnly(true);
    details->setMaximumHeight(220);
    details->hide();
    root->addWidget(details);

    auto* buttons = new QDialogButtonBox(this);

    auto* proceed = new QPushButton(mode == Mode::Install ? tr("Install") : tr("Remove"), buttons);
    proceed->setProperty("accent", true);
    proceed->setDefault(true);
    buttons->addButton(proceed, QDialogButtonBox::AcceptRole);

    auto* cancel = new QPushButton(tr("Cancel"), buttons);
    buttons->addButton(cancel, QDialogButtonBox::RejectRole);

    if (hasConflicts) {
        auto* replace = new QPushButton(tr("Replace"), buttons);
        buttons->addButton(replace, QDialogButtonBox::AcceptRole);
        connect(replace, &QPushButton::clicked, this, [this] { m_replace = true; accept(); });
        cancel->setDefault(true);
        proceed->setDefault(false);
    } else {
        cancel->setDefault(true);
    }

    auto* detailsBtn = new QPushButton(tr("Show Details"), buttons);
    detailsBtn->setCheckable(true);
    buttons->addButton(detailsBtn, QDialogButtonBox::ActionRole);
    connect(detailsBtn, &QPushButton::toggled, details, &QWidget::setVisible);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    root->addWidget(buttons);
}

bool ConfirmDialog::confirmQuestion(const QString& title, const QString& text, QWidget* parent) {
    QMessageBox box(QMessageBox::Question, title, text, QMessageBox::Yes | QMessageBox::No, parent);
    box.setDefaultButton(QMessageBox::No);
    return box.exec() == QMessageBox::Yes;
}
