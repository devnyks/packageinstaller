#include "console_page.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QPushButton>
#include <QVBoxLayout>

ConsolePage::ConsolePage(QWidget* parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("consoleOutput"));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(12);

    auto* header = new QHBoxLayout();
    auto* title = new QLabel(tr("Console Output"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    header->addWidget(title);
    header->addStretch(1);
    m_cancel = new QPushButton(tr("Cancel"), this);
    header->addWidget(m_cancel);
    m_stop = new QPushButton(tr("Stop"), this);
    m_stop->setProperty("danger", true);
    header->addWidget(m_stop);
    root->addLayout(header);

    m_output = new QPlainTextEdit(this);
    m_output->setReadOnly(true);
    m_output->setObjectName(QStringLiteral("outputBox"));
    QFont mono(QStringLiteral("monospace"));
    mono.setStyleHint(QFont::Monospace);
    m_output->setFont(mono);
    m_output->setLineWrapMode(QPlainTextEdit::NoWrap);
    root->addWidget(m_output, 1);

    auto* inputRow = new QHBoxLayout();
    m_input = new QLineEdit(this);
    m_input->setPlaceholderText(tr("Type a response and press Enter (for interactive prompts)..."));
    inputRow->addWidget(m_input);
    auto* enter = new QPushButton(tr("Enter"), this);
    inputRow->addWidget(enter);
    root->addLayout(inputRow);

    connect(m_input, &QLineEdit::returnPressed, this, [this] {
        const QByteArray data = m_input->text().toUtf8() + "\n";
        m_output->appendPlainText(m_input->text() + "\n");
        m_input->clear();
        emit inputSubmitted(data);
    });
    connect(enter, &QPushButton::clicked, m_input, &QLineEdit::returnPressed);
    connect(m_cancel, &QPushButton::clicked, this, &ConsolePage::cancelRequested);
    connect(m_stop, &QPushButton::clicked, this, &ConsolePage::cancelRequested);
    setRunning(false);
}

void ConsolePage::appendOutput(const QString& text) {
    m_output->moveCursor(QTextCursor::End);
    m_output->insertPlainText(text);
    m_output->verticalScrollBar()->setValue(m_output->verticalScrollBar()->maximum());
}

void ConsolePage::appendError(const QString& text) {
    appendOutput(text);
}

void ConsolePage::clear() {
    m_output->clear();
}

void ConsolePage::setRunning(bool running) {
    m_input->setEnabled(running);
    m_stop->setEnabled(running);
}

void ConsolePage::setTitle(const QString& text) {
    if (auto* title = findChild<QLabel*>()) {
        title->setText(text);
    }
}
