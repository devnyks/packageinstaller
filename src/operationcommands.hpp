#pragma once

#include <QStringList>

struct ProcessSpec {
    QString program;
    QStringList arguments;
};

namespace OperationCommands {

ProcessSpec nativeMutation(const QString& action, const QStringList& targets);
ProcessSpec flatpakMutation(const QString& scope, const QString& action, const QString& remote, const QStringList& targets);
ProcessSpec flatpakRemoteAdd(const QString& scope, const QString& name, const QString& url, bool verified = false);
ProcessSpec flatpakRemoteRemove(const QString& scope, const QString& name);
ProcessSpec flatpakAppstreamRefresh(const QString& scope);

} // namespace OperationCommands
