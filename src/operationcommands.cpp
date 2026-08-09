#include "operationcommands.hpp"

namespace OperationCommands {

ProcessSpec nativeMutation(const QString& action, const QStringList& targets) {
    ProcessSpec spec{QStringLiteral("pkexec"), {QStringLiteral("pacman")}};
    if (action == QStringLiteral("remove") || action == QStringLiteral("remove-orphans")) {
        spec.arguments << QStringLiteral("-R") << targets;
    } else if (action == QStringLiteral("upgrade") && targets.isEmpty()) {
        spec.arguments << QStringLiteral("-Syu");
    } else {
        spec.arguments << QStringLiteral("-S") << targets;
    }
    return spec;
}

ProcessSpec flatpakMutation(const QString& scope, const QString& action, const QString& remote, const QStringList& targets) {
    const bool system = scope != QStringLiteral("user");
    ProcessSpec spec{system ? QStringLiteral("pkexec") : QStringLiteral("flatpak"), {}};
    QStringList arguments{system ? QStringLiteral("flatpak") : QString(), system ? QStringLiteral("--system") : QStringLiteral("--user")};
    arguments.removeAll(QString());
    if (action == QStringLiteral("flatpak-update")) {
        arguments << QStringLiteral("update") << QStringLiteral("-y");
    } else if (action == QStringLiteral("flatpak-update-selected")) {
        arguments << QStringLiteral("update") << QStringLiteral("-y") << targets;
    } else if (action == QStringLiteral("flatpak-unused")) {
        arguments << QStringLiteral("uninstall") << QStringLiteral("--unused") << QStringLiteral("-y");
    } else if (action == QStringLiteral("flatpakref")) {
        arguments << QStringLiteral("install") << QStringLiteral("-y") << QStringLiteral("--from") << targets.value(0);
    } else if (action == QStringLiteral("remove")) {
        arguments << QStringLiteral("uninstall") << QStringLiteral("-y") << targets;
    } else {
        arguments << QStringLiteral("install") << QStringLiteral("-y");
        if (!remote.isEmpty()) {
            arguments << remote;
        }
        arguments << targets;
    }
    spec.arguments = arguments;
    return spec;
}

ProcessSpec flatpakRemoteAdd(const QString& scope, const QString& name, const QString& url, bool verified) {
    const bool system = scope != QStringLiteral("user");
    ProcessSpec spec{system ? QStringLiteral("pkexec") : QStringLiteral("flatpak"), {}};
    spec.arguments = {system ? QStringLiteral("flatpak") : QString(), system ? QStringLiteral("--system") : QStringLiteral("--user"), QStringLiteral("remote-add"), QStringLiteral("--if-not-exists")};
    if (verified) {
        spec.arguments << QStringLiteral("--subset=verified");
    }
    spec.arguments << name << url;
    spec.arguments.removeAll(QString());
    return spec;
}

ProcessSpec flatpakRemoteRemove(const QString& scope, const QString& name) {
    const bool system = scope != QStringLiteral("user");
    ProcessSpec spec{system ? QStringLiteral("pkexec") : QStringLiteral("flatpak"), {}};
    spec.arguments = {system ? QStringLiteral("flatpak") : QString(), system ? QStringLiteral("--system") : QStringLiteral("--user"), QStringLiteral("remote-delete"), name};
    spec.arguments.removeAll(QString());
    return spec;
}

ProcessSpec flatpakAppstreamRefresh(const QString& scope) {
    const bool system = scope != QStringLiteral("user");
    ProcessSpec spec{system ? QStringLiteral("pkexec") : QStringLiteral("flatpak"), {}};
    spec.arguments = {system ? QStringLiteral("flatpak") : QString(), system ? QStringLiteral("--system") : QStringLiteral("--user"), QStringLiteral("update"), QStringLiteral("--appstream")};
    spec.arguments.removeAll(QString());
    return spec;
}

} // namespace OperationCommands
