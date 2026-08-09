#include "flatpak.h"

#include "cmd.h"
#include "logging.h"

#include <QRegularExpression>
#include <QtConcurrent>

FlatpakBackend::FlatpakBackend(QObject* parent)
    : QObject(parent) {}

void FlatpakBackend::setUserScope(bool userScope) {
    m_userScope = userScope;
}

QStringList FlatpakBackend::listRemotes() {
    const QString out = runShellCommand(
        QStringLiteral("flatpak remote-list %1| cut -f1").arg(scopeArgs()));
    QStringList list;
    for (const QString& line : out.split(QLatin1Char('\n'))) {
        const QString name = line.trimmed();
        if (!name.isEmpty()) {
            list << name;
        }
    }
    return list;
}

QList<FlatpakBackend::Ref> FlatpakBackend::listAvailable(const QString& remote, bool runtimes) {
    QList<Ref> result;

    const QString columns = runtimes ? QStringLiteral("branch,ref,installed-size")
                                     : QStringLiteral("ver,ref,installed-size");
    const QString type = runtimes ? QStringLiteral("--runtime ") : QStringLiteral("--app ");
    const QString cmd = QStringLiteral("flatpak remote-ls %1%2 --arch=x86_64 %3--columns=%4 2>/dev/null")
                            .arg(scopeArgs(), remote, type, columns);
    const QString out = runShellCommand(cmd);

    for (const QString& rawLine : out.split(QLatin1Char('\n'))) {
        QString line = rawLine.trimmed();
        if (line.isEmpty()) {
            continue;
        }
        QString size = line.section(QLatin1Char('\t'), -1);
        QString version = line.section(QLatin1Char('\t'), 0, 0);
        QString fullRef = line.section(QLatin1Char('\t'), 1, 1);  // e.g. app/org.foo/x86_64/stable
        QString appId = fullRef.section(QLatin1Char('/'), 1);
        if (version.isEmpty()) {
            version = appId.section(QLatin1Char('/'), -1);
        }
        if (appId.isEmpty()) {
            continue;
        }
        const QString shortName = appId.section(QLatin1Char('.'), -1);
        // skip Locale/Sources/Debug extension points (reference parity)
        if (shortName == QLatin1String("Locale") || shortName == QLatin1String("Sources")
            || shortName == QLatin1String("Debug")) {
            continue;
        }
        Ref r;
        r.appId = appId;
        r.shortName = shortName;
        r.version = version;
        r.size = size;
        r.isRuntime = runtimes;
        // strip the leading "app/" or "runtime/" prefix so it matches the
        // format reported by `flatpak list --columns=ref`
        r.normalizedRef = fullRef.section(QLatin1Char('/'), 1);
        result.append(std::move(r));
    }
    return result;
}

QStringList FlatpakBackend::listInstalledRefs(bool runtimes) {
    const QString type = runtimes ? QStringLiteral("--runtime ") : QStringLiteral("--app ");
    const QString out = runShellCommand(
        QStringLiteral("flatpak list %1 %2--columns=ref 2>/dev/null").arg(scopeArgs(), type));
    QStringList refs;
    for (const QString& line : out.split(QLatin1Char('\n'))) {
        QString ref = line.trimmed().remove(QLatin1Char(' '));
        if (ref.isEmpty()) {
            continue;
        }
        // `flatpak list` refs have no app/ runtime/ prefix; normalize to match
        // remote-ls refs (which include it).
        if (ref.startsWith(QLatin1String("app/")) || ref.startsWith(QLatin1String("runtime/"))) {
            ref = ref.section(QLatin1Char('/'), 1);
        }
        refs << ref;
    }
    return refs;
}

QString FlatpakBackend::listInstalledSize() {
    const QString out = runShellCommand(
        QStringLiteral("flatpak list %1--columns app,size 2>/dev/null").arg(scopeArgs()));

    auto parseSize = [](const QString& text) -> double {
        QString num = text;
        num.replace(QLatin1Char(','), QLatin1Char('.'));
        const QString unit = num.section(QLatin1Char(' '), 1).trimmed().toUpper();
        double value = num.section(QLatin1Char(' '), 0, 0).toDouble();
        if (unit == QLatin1String("KB")) {
            value *= 1024;
        } else if (unit == QLatin1String("MB")) {
            value *= 1024 * 1024;
        } else if (unit == QLatin1String("GB")) {
            value *= 1024 * 1024 * 1024;
        }
        return value;
    };

    double bytes = 0;
    for (const QString& line : out.split(QLatin1Char('\n'))) {
        bytes += parseSize(line.section(QLatin1Char('\t'), 1));
    }
    if (bytes < 1024) {
        return QStringLiteral("%1 bytes").arg(bytes, 0, 'f', 0);
    }
    if (bytes < 1024 * 1024) {
        return QStringLiteral("%1 KB").arg(bytes / 1024, 0, 'f', 0);
    }
    if (bytes < 1024 * 1024 * 1024) {
        return QStringLiteral("%1 MB").arg(bytes / (1024 * 1024), 0, 'f', 1);
    }
    return QStringLiteral("%1 GB").arg(bytes / (1024 * 1024 * 1024), 0, 'f', 2);
}

void FlatpakBackend::ensureFlathubRemotes() {
    runShellCommand(QStringLiteral(
        "flatpak --user remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo "
        "&& flatpak --user remote-add --if-not-exists --subset=verified flathub-verified "
        "https://flathub.org/repo/flathub.flatpakrepo"));
}

void FlatpakBackend::updateAppstream() {
    if (m_appstreamUpdated) {
        return;
    }
    m_appstreamUpdated = true;
    runShellCommand(QStringLiteral("flatpak update --appstream"));
}

QString FlatpakBackend::installCommand(const QString& remote, const QStringList& appIds) const {
    return QStringLiteral("socat SYSTEM:'flatpak install -y %1%2 %3',stderr STDIO")
        .arg(scopeArgs(), remote, appIds.join(QLatin1Char(' ')));
}

QString FlatpakBackend::uninstallCommand(const QString& appId) const {
    return QStringLiteral("socat SYSTEM:'flatpak uninstall -y %1%2',stderr STDIO")
        .arg(scopeArgs(), appId);
}

QString FlatpakBackend::updateAllCommand() const {
    return QStringLiteral("socat SYSTEM:'flatpak update %1',pty STDIO")
        .arg(m_userScope ? QStringLiteral("--user") : QStringLiteral("--system"));
}

QString FlatpakBackend::removeUnusedCommand() const {
    return QStringLiteral("socat SYSTEM:'flatpak uninstall --unused -y %1',pty STDIO")
        .arg(m_userScope ? QStringLiteral("--user") : QStringLiteral("--system"));
}

QString FlatpakBackend::installRefCommand(const QString& location) const {
    QString ref = location;
    ref.replace(QLatin1Char(':'), QStringLiteral("\\:"));
    return QStringLiteral("socat SYSTEM:'flatpak install -y %1--from %2',stderr STDIO")
        .arg(scopeArgs(), ref);
}

QString FlatpakBackend::addRemoteCommand(const QString& name, const QString& url) const {
    return QStringLiteral("flatpak remote-add %1--if-not-exists %2 %3")
        .arg(scopeArgs(), name, url);
}

QString FlatpakBackend::removeRemoteCommand(const QString& name) const {
    const QString flag = m_userScope ? QStringLiteral("--user") : QStringLiteral("--system");
    return QStringLiteral("flatpak remote-delete %1 %2").arg(name, flag);
}
