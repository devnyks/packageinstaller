#pragma once

#include "appstream_provider.h"

#include <QWidget>

class QLabel;
class QPushButton;
class QNetworkAccessManager;
class QNetworkReply;
class QScrollArea;
class QVBoxLayout;

/// Detail panel shown next to a selected package in the Discover page:
/// AppStream screenshot (loaded asynchronously with a cache), description,
/// metadata and action buttons.
class PackageDetail : public QWidget {
    Q_OBJECT

public:
    enum class State { NotInstalled, Installed, Upgradable };

    explicit PackageDetail(QWidget* parent = nullptr);

    void showPackage(const QString& name, const QString& version,
        const QString& description, const QStringList& extras,
        const AppStreamProvider::Component* appstream, State state);
    void clear();

    QString packageName() const { return m_name; }

signals:
    void actionRequested(const QString& name, int action);  // 0 install, 1 uninstall

private:
    void loadScreenshot(const QString& url);
    void render();

    QString m_name;
    QString m_version;
    QString m_description;
    QStringList m_extras;
    QStringList m_screenshots;
    State m_state = State::NotInstalled;

    QLabel* m_icon = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_versionLabel = nullptr;
    QLabel* m_badge = nullptr;
    QLabel* m_screenshot = nullptr;
    QLabel* m_descLabel = nullptr;
    QLabel* m_extrasLabel = nullptr;
    QPushButton* m_action = nullptr;
    QNetworkAccessManager* m_network = nullptr;
    QVBoxLayout* m_layout = nullptr;
};
