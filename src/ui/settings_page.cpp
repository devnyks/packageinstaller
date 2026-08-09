#include "settings_page.h"

#include "theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSettings>
#include <QVBoxLayout>

SettingsPage::SettingsPage(QWidget* parent)
    : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(16);

    auto* title = new QLabel(tr("Settings"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    root->addWidget(title);

    auto* section = new QLabel(tr("Appearance"), this);
    section->setObjectName(QStringLiteral("sectionTitle"));
    root->addWidget(section);

    auto* form = new QFormLayout();
    form->setSpacing(12);
    m_theme = new QComboBox(this);
    m_theme->addItem(tr("System"), QStringLiteral("system"));
    m_theme->addItem(tr("Light"), QStringLiteral("light"));
    m_theme->addItem(tr("Dark"), QStringLiteral("dark"));
    form->addRow(tr("Theme:"), m_theme);
    root->addLayout(form);

    auto* section2 = new QLabel(tr("Features"), this);
    section2->setObjectName(QStringLiteral("sectionTitle"));
    root->addWidget(section2);

    m_showFlatpak = new QCheckBox(tr("Show the Flatpak page"), this);
    root->addWidget(m_showFlatpak);
    m_disableWarning = new QCheckBox(tr("Do not show the Flatpak warning again"), this);
    root->addWidget(m_disableWarning);

    root->addStretch(1);

    auto* section3 = new QLabel(tr("About"), this);
    section3->setObjectName(QStringLiteral("sectionTitle"));
    root->addWidget(section3);
    m_version = new QLabel(this);
    m_version->setObjectName(QStringLiteral("pageSubtitle"));
    root->addWidget(m_version);
    auto* about = new QLabel(
        tr("A modern package catalog for CachyOS. Package operations run through "
           "polkit (pkexec) and the official pacman / flatpak tools."),
        this);
    about->setWordWrap(true);
    about->setObjectName(QStringLiteral("pageSubtitle"));
    root->addWidget(about);

    connect(m_theme, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        emit themeChanged(m_theme->currentData().toString());
    });
    connect(m_showFlatpak, &QCheckBox::toggled, this, [this](bool on) {
        emit flatpakVisibilityChanged(on);
    });
    // the "do not show warning" flag persists directly
    connect(m_disableWarning, &QCheckBox::toggled, this, [](bool on) {
        QSettings().setValue(QStringLiteral("disableWarning"), on);
    });
}

void SettingsPage::loadSettings() {
    QSettings s;
    const QString themeMode = s.value(QStringLiteral("theme"), QStringLiteral("system")).toString();
    const int idx = m_theme->findData(themeMode);
    m_theme->setCurrentIndex(idx >= 0 ? idx : 0);
    m_showFlatpak->setChecked(s.value(QStringLiteral("showFlatpak"), true).toBool());
    m_disableWarning->setChecked(s.value(QStringLiteral("disableWarning"), false).toBool());
}

void SettingsPage::setAppVersion(const QString& version) {
    m_version->setText(tr("CachyOS Package Installer %1").arg(version));
}
