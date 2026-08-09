#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

/// AppStream metadata provider (libappstream AsPool). Loads the OS catalog,
/// OS metainfo and the Flatpak catalog in a worker thread, then serves
/// read-only lookups on the main thread:
///  - package name  -> component (name, summary, description, icon, screenshots)
///  - component id  -> component (flatpak apps / runtimes)
///
/// This is the Linux-native source for application metadata and screenshots
/// used by the Discover page cards and detail panels.
class AppStreamProvider : public QObject {
    Q_OBJECT

public:
    struct Component {
        QString id;
        QString name;
        QString summary;
        QString description;
        QString iconName;       // theme icon name (native packages)
        QString iconFile;       // local icon file (flatpak catalog)
        QString developer;
        QString homepage;
        QString projectUrl;
        QStringList screenshots;  // screenshot URLs
        bool fromFlatpak = false;
    };

    explicit AppStreamProvider(QObject* parent = nullptr);
    ~AppStreamProvider() override;

    /// Start background pool load. Emits `ready` when done.
    void loadAsync();

    bool isReady() const { return m_ready; }

    /// Look up a component by AppStream id (e.g. org.videolan.VLC).
    std::optional<Component> byId(const QString& id) const;

    /// Look up a component that ships `pkgname` (native packages).
    std::optional<Component> byPackage(const QString& pkgname) const;

signals:
    void ready();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    bool m_ready = false;
};
