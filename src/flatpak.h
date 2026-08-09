#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

/// Flatpak backend. Mirrors the reference Flatpak integration:
///  - user/system scopes,
///  - remote-ls based app/runtime discovery (with AppStream metadata),
///  - remote add/remove, flatpakref installs,
///  - privileged flows run through socat PTY wrappers so interactive
///    prompts reach the console page (reference parity).
class FlatpakBackend : public QObject {
    Q_OBJECT

public:
    struct Ref {
        QString appId;        // "org.foo" (remote-ls ref without app/ prefix)
        QString shortName;    // "foo"
        QString version;      // version / branch column
        QString size;         // installed-size column (display text)
        bool isRuntime = false;
        bool installed = false;
        QString normalizedRef;  // "org.foo/x86_64/stable" (prefix stripped) for installed matching
    };

    explicit FlatpakBackend(QObject* parent = nullptr);

    void setUserScope(bool userScope);
    bool isUserScope() const { return m_userScope; }

    /// Remote names for the current scope.
    QStringList listRemotes();

    /// Apps (or runtimes) available in `remote` for the current scope,
    /// annotated with installed state from `installedRefs`.
    QList<Ref> listAvailable(const QString& remote, bool runtimes);

    /// Installed app/runtime refs ("org.foo/x86_64/stable") for the current scope.
    QStringList listInstalledRefs(bool runtimes);

    /// Total installed size line (reference `listSizeInstalledFP`).
    QString listInstalledSize();

    /// Ensure the current scope has the standard flathub remotes
    /// (reference behavior when switching to the user scope).
    void ensureFlathubRemotes();

    /// Refresh remote appstream data once per session (reference parity).
    void updateAppstream();

    // ---- privileged operations (streamed to console by the caller) ----

    QString installCommand(const QString& remote, const QStringList& appIds) const;
    QString uninstallCommand(const QString& appId) const;
    QString updateAllCommand() const;
    QString removeUnusedCommand() const;
    QString installRefCommand(const QString& location) const;
    QString addRemoteCommand(const QString& name, const QString& url) const;
    QString removeRemoteCommand(const QString& name) const;

private:
    QString scopeArgs() const { return m_userScope ? QStringLiteral("--user ") : QStringLiteral("--system "); }
    bool m_userScope = false;
    bool m_appstreamUpdated = false;
};
