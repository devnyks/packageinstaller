#include "package_detail.h"

#include <QHBoxLayout>
#include <QCryptographicHash>
#include <QIcon>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QPushButton>
#include <QStyle>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QStyle>
#include <QUrl>
#include <QVBoxLayout>


namespace {
QString screenshotCachePath(const QString& url) {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
        + QStringLiteral("/screenshots");
    QDir().mkpath(dir);
    return dir + QLatin1Char('/') + QString::fromLatin1(QCryptographicHash::hash(
        url.toUtf8(), QCryptographicHash::Sha1).toHex()) + QStringLiteral(".png");
}
}  // namespace

PackageDetail::PackageDetail(QWidget* parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("detailPanel"));
    setAttribute(Qt::WA_StyledBackground, true);
    setMinimumWidth(340);
    setMaximumWidth(440);
    m_network = new QNetworkAccessManager(this);

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(20, 20, 20, 20);
    m_layout->setSpacing(12);

    auto* head = new QHBoxLayout();
    head->setSpacing(12);
    m_icon = new QLabel(this);
    m_icon->setFixedSize(64, 64);
    head->addWidget(m_icon, 0, Qt::AlignTop);

    auto* titleCol = new QVBoxLayout();
    m_title = new QLabel(this);
    m_title->setObjectName(QStringLiteral("pageTitle"));
    m_title->setWordWrap(true);
    titleCol->addWidget(m_title);
    m_versionLabel = new QLabel(this);
    m_versionLabel->setObjectName(QStringLiteral("pageSubtitle"));
    titleCol->addWidget(m_versionLabel);
    m_badge = new QLabel(this);
    titleCol->addWidget(m_badge);
    titleCol->addStretch(1);
    head->addLayout(titleCol, 1);
    m_layout->addLayout(head);

    m_screenshot = new QLabel(this);
    m_screenshot->setAlignment(Qt::AlignCenter);
    m_screenshot->setMinimumHeight(180);
    m_screenshot->setMaximumHeight(260);
    m_screenshot->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_screenshot->setObjectName(QStringLiteral("card"));
    m_screenshot->setAttribute(Qt::WA_StyledBackground, true);
    m_layout->addWidget(m_screenshot);

    m_descLabel = new QLabel(this);
    m_descLabel->setWordWrap(true);
    m_descLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_layout->addWidget(m_descLabel);

    m_extrasLabel = new QLabel(this);
    m_extrasLabel->setObjectName(QStringLiteral("pageSubtitle"));
    m_extrasLabel->setWordWrap(true);
    m_extrasLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_layout->addWidget(m_extrasLabel);

    m_layout->addStretch(1);

    m_action = new QPushButton(this);
    m_action->setFixedHeight(34);
    m_layout->addWidget(m_action);
    connect(m_action, &QPushButton::clicked, this, [this] {
        emit actionRequested(m_name, m_state == State::Installed ? 1 : 0);
    });
}

void PackageDetail::showPackage(const QString& name, const QString& version,
    const QString& description, const QStringList& extras,
    const AppStreamProvider::Component* appstream, State state) {
    m_name = name;
    m_version = version;
    m_description = description;
    m_extras = extras;
    m_state = state;
    m_screenshots.clear();

    if (appstream) {
        m_title->setText(appstream->name);
        m_descLabel->setText(appstream->description.isEmpty() ? description : appstream->description);
        if (!appstream->iconFile.isEmpty()) {
            m_icon->setPixmap(QPixmap(appstream->iconFile).scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else if (!appstream->iconName.isEmpty()) {
            m_icon->setPixmap(QIcon::fromTheme(appstream->iconName).pixmap(64, 64));
        } else {
            m_icon->setPixmap(QIcon(QStringLiteral(":/icons/app-icon-64.png")).pixmap(64, 64));
        }
        m_screenshots = appstream->screenshots;
    } else {
        m_title->setText(name);
        m_descLabel->setText(description);
        m_icon->setPixmap(QIcon::fromTheme(QStringLiteral("package-x-generic"),
            QIcon(QStringLiteral(":/icons/app-icon-64.png"))).pixmap(64, 64));
    }
    m_versionLabel->setText(tr("Version %1").arg(version));

    QStringList meta;
    for (const QString& e : extras) {
        meta << e;
    }
    if (appstream && !appstream->developer.isEmpty()) {
        meta.prepend(tr("Developer: %1").arg(appstream->developer));
    }
    if (appstream && !appstream->homepage.isEmpty()) {
        meta.prepend(tr("Homepage: %1").arg(appstream->homepage));
    }
    m_extrasLabel->setText(meta.join(QLatin1Char('\n')));

    m_badge->setProperty("status", state == State::Upgradable ? QStringLiteral("upgradable")
            : (state == State::Installed ? QStringLiteral("installed") : QStringLiteral("notInstalled")));
    m_badge->setText(state == State::Upgradable ? tr("Update available")
            : (state == State::Installed ? tr("Installed") : tr("Not installed")));
    m_badge->style()->unpolish(m_badge);
    m_badge->style()->polish(m_badge);

    m_action->setText(state == State::Installed ? tr("Remove") : (state == State::Upgradable ? tr("Update") : tr("Install")));
    m_action->setProperty("accent", state != State::Installed);
    m_action->setProperty("danger", state == State::Installed);
    m_action->style()->unpolish(m_action);
    m_action->style()->polish(m_action);

    if (!m_screenshots.isEmpty()) {
        loadScreenshot(m_screenshots.first());
    } else {
        m_screenshot->clear();
        m_screenshot->setFixedHeight(120);
    }
}

void PackageDetail::clear() {
    m_name.clear();
    m_title->clear();
    m_versionLabel->clear();
    m_descLabel->clear();
    m_extrasLabel->clear();
    m_badge->clear();
    m_screenshot->clear();
    m_icon->clear();
    m_action->setText(tr("Install"));
    m_action->setEnabled(false);
}

void PackageDetail::loadScreenshot(const QString& url) {
    m_screenshot->setFixedHeight(260);
    m_screenshot->setText(tr("Loading screenshot..."));

    const QString cachePath = screenshotCachePath(url);
    if (QFile::exists(cachePath)) {
        QPixmap pm(cachePath);
        if (!pm.isNull()) {
            m_screenshot->setPixmap(pm.scaled(m_screenshot->width(), 240, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            return;
        }
    }

    QNetworkRequest req{QUrl(url)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_network->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, url] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_screenshot->setText(tr("Screenshot unavailable (offline)"));
            return;
        }
        const QByteArray data = reply->readAll();
        QPixmap pm;
        if (!pm.loadFromData(data) || pm.isNull()) {
            m_screenshot->setText(tr("Screenshot unavailable"));
            return;
        }
        pm.save(screenshotCachePath(url), "PNG");
        m_screenshot->setPixmap(pm.scaled(m_screenshot->width(), 240, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    });
}
