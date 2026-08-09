#pragma once

#include <QWidget>

class QComboBox;
class QCheckBox;
class QLabel;

/// Settings page: theme (System/Light/Dark), Flatpak page visibility,
/// warning preferences and application info.
class SettingsPage : public QWidget {
    Q_OBJECT

public:
    explicit SettingsPage(QWidget* parent = nullptr);

    void loadSettings();
    void setAppVersion(const QString& version);

signals:
    void themeChanged(const QString& mode);
    void flatpakVisibilityChanged(bool visible);

private:
    QComboBox* m_theme = nullptr;
    QCheckBox* m_showFlatpak = nullptr;
    QCheckBox* m_disableWarning = nullptr;
    QLabel* m_version = nullptr;
};
