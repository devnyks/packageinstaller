#pragma once

#include <QObject>
#include <QProcess>
#include <QString>

/// Asynchronous privileged-process runner (pkexec pacman / flatpak / socat).
/// Mirrors reference `Cmd` semantics: runs via /bin/bash -c, streams stdout
/// and stderr, supports interactive stdin (PTY via socat) and termination.
class Cmd : public QObject {
    Q_OBJECT

public:
    explicit Cmd(QObject* parent = nullptr);

    /// Run a command asynchronously; output is streamed through
    /// `outputAvailable`. Returns false (and logs) if a process is already
    /// running. Completion reported via `finished`.
    bool run(const QString& cmd);

    /// Write data to the process stdin (interactive prompts).
    void write(const QByteArray& data);

    void terminate();
    void halt();

    bool isRunning() const { return m_proc.state() != QProcess::NotRunning; }

signals:
    void outputAvailable(const QString& out);
    void errorAvailable(const QString& err);
    void finished(bool ok, int exitCode);

private:
    QProcess m_proc;
};

/// Blocking shell command helper, safe to call from worker threads (spawns
/// its own process). Returns trimmed stdout+stderr combined; `ok` receives
/// the exit status.
QString runShellCommand(const QString& cmd, bool* ok = nullptr);
