#pragma once

#include <QProcess>

class ProcessRunner final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)

public:
    explicit ProcessRunner(QObject* parent = nullptr);

    bool running() const { return m_process.state() != QProcess::NotRunning; }
    QString capturedOutput() const { return m_output; }

    bool start(const QString& program, const QStringList& arguments);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void writeInput(const QString& text);

signals:
    void runningChanged();
    void started();
    void outputAvailable(const QString& text);
    void errorAvailable(const QString& text);
    void completed(int exitCode, QProcess::ExitStatus status, const QString& output, bool cancelled);

private slots:
    void readOutput();
    void readError();
    void processFinished(int exitCode, QProcess::ExitStatus status);

private:
    void appendOutput(const QByteArray& bytes, bool error);

    QProcess m_process;
    QString m_output;
    bool m_cancelRequested = false;
    bool m_completionEmitted = false;
};
