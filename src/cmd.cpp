#include "cmd.h"

#include "logging.h"

Cmd::Cmd(QObject* parent)
    : QObject(parent), m_proc(this) {
    m_proc.setProcessChannelMode(QProcess::SeparateChannels);
    connect(&m_proc, &QProcess::readyReadStandardOutput, this, [this] {
        emit outputAvailable(QString::fromUtf8(m_proc.readAllStandardOutput()));
    });
    connect(&m_proc, &QProcess::readyReadStandardError, this, [this] {
        emit errorAvailable(QString::fromUtf8(m_proc.readAllStandardError()));
    });
    connect(&m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
        [this](int exitCode, QProcess::ExitStatus status) {
            emit finished(status == QProcess::NormalExit && exitCode == 0, exitCode);
        });
}

bool Cmd::run(const QString& cmd) {
    if (m_proc.state() != QProcess::NotRunning) {
        logging::warn(QStringLiteral("Cmd already running, ignoring: %1").arg(cmd));
        return false;
    }
    logging::debug(QStringLiteral("RUN: %1").arg(cmd));
    m_proc.start(QStringLiteral("/bin/bash"), {QStringLiteral("-c"), cmd});
    return m_proc.waitForStarted(3000);
}

void Cmd::write(const QByteArray& data) {
    if (m_proc.state() == QProcess::Running) {
        m_proc.write(data);
    }
}

void Cmd::terminate() {
    m_proc.terminate();
}

void Cmd::halt() {
    if (m_proc.state() == QProcess::NotRunning) {
        return;
    }
    m_proc.terminate();
    if (!m_proc.waitForFinished(5000)) {
        m_proc.kill();
        m_proc.waitForFinished(1000);
    }
}

QString runShellCommand(const QString& cmd, bool* ok) {
    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(QStringLiteral("/bin/bash"), {QStringLiteral("-c"), cmd});
    if (!proc.waitForStarted(3000)) {
        if (ok) {
            *ok = false;
        }
        return {};
    }
    proc.waitForFinished(-1);
    const QString out = QString::fromUtf8(proc.readAll());
    if (ok) {
        *ok = proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;
    }
    return out;
}
