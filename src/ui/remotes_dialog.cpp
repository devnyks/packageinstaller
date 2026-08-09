#include "remotes_dialog.h"

#include "cmd.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

RemotesDialog::RemotesDialog(FlatpakBackend* backend, QWidget* parent)
    : QDialog(parent), m_backend(backend) {
    setWindowTitle(tr("Manage Flatpak Remotes"));
    setMinimumWidth(520);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 18);
    root->setSpacing(12);

    auto* intro = new QLabel(tr("Add or remove flatpak remotes (repos), or install apps using a flatpakref URL or path."), this);
    intro->setWordWrap(true);
    root->addWidget(intro);

    auto* form = new QFormLayout();
    form->setSpacing(10);

    m_scope = new QComboBox(this);
    m_scope->addItem(tr("For all users"));
    m_scope->addItem(tr("For current user"));
    form->addRow(tr("Scope:"), m_scope);

    m_remote = new QComboBox(this);
    form->addRow(tr("Remote:"), m_remote);

    m_addUrl = new QLineEdit(this);
    m_addUrl->setPlaceholderText(tr("enter Flatpak remote URL"));
    form->addRow(tr("Add remote:"), m_addUrl);

    m_installRefEdit = new QLineEdit(this);
    m_installRefEdit->setPlaceholderText(tr("enter Flatpakref location to install app"));
    form->addRow(tr("Install flatpakref:"), m_installRefEdit);
    root->addLayout(form);

    auto* row = new QHBoxLayout();
    row->addStretch(1);
    auto* remove = new QPushButton(tr("Remove remote"), this);
    auto* add = new QPushButton(tr("Add remote"), this);
    auto* install = new QPushButton(tr("Install app"), this);
    install->setProperty("accent", true);
    auto* close = new QPushButton(tr("Close"), this);
    row->addWidget(remove);
    row->addWidget(add);
    row->addWidget(install);
    row->addWidget(close);
    root->addLayout(row);

    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    connect(add, &QPushButton::clicked, this, &RemotesDialog::addRemote);
    connect(remove, &QPushButton::clicked, this, &RemotesDialog::removeRemote);
    connect(install, &QPushButton::clicked, this, &RemotesDialog::requestInstall);
    connect(m_scope, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { listRemotes(); });

    listRemotes();
}

void RemotesDialog::listRemotes() {
    m_userScope = m_scope->currentIndex() == 1;
    m_backend->setUserScope(m_userScope);
    m_remote->clear();
    const QStringList remotes = m_backend->listRemotes();
    for (const QString& r : remotes) {
        m_remote->addItem(r.section(QLatin1Char('\t'), 0, 0));
    }
}

void RemotesDialog::addRemote() {
    const QString location = m_addUrl->text().trimmed();
    if (location.isEmpty()) {
        return;
    }
    const QString name = location.section(QLatin1Char('/'), -1).section(QLatin1Char('.'), 0, 0);
    bool rc = false;
    setCursor(QCursor(Qt::BusyCursor));
    runShellCommand(m_backend->addRemoteCommand(name, location), &rc);
    setCursor(QCursor(Qt::ArrowCursor));
    if (rc) {
        m_changed = true;
        m_addUrl->clear();
        QMessageBox::information(this, tr("Success"), tr("Remote added successfully"));
        listRemotes();
    } else {
        QMessageBox::critical(this, tr("Error adding remote"),
            tr("Could not add remote - command returned an error. Please double-check the remote address and try again"));
    }
}

void RemotesDialog::removeRemote() {
    if (m_remote->currentText() == QLatin1String("flathub")) {
        QMessageBox::information(this, tr("Not removable"),
            tr("Flathub is the main Flatpak remote and won't be removed"));
        return;
    }
    if (m_remote->currentText().isEmpty()) {
        return;
    }
    runShellCommand(m_backend->removeRemoteCommand(m_remote->currentText().section(QLatin1Char('\t'), 0, 0)));
    m_changed = true;
    listRemotes();
}

void RemotesDialog::requestInstall() {
    m_installRef = m_installRefEdit->text().trimmed();
    m_userScope = m_scope->currentIndex() == 1;
    accept();
}
