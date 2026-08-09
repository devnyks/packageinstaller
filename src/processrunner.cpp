#include "processrunner.hpp"

#include <QTimer>
#include <QDebug>

ProcessRunner::ProcessRunner(QObject* parent)
    : QObject(parent) {
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    connect(&m_process, &QProcess::started, this, [this] {
        emit runningChanged();
        emit started();
    });
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &ProcessRunner::readOutput);
    connect(&m_process, &QProcess::readyReadStandardError, this, &ProcessRunner::readError);
    connect(&m_process, &QProcess::finished, this, &ProcessRunner::processFinished);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            const auto message = m_process.errorString();
            appendOutput(message.toUtf8() + '\n', true);
            if (!m_completionEmitted) {
                m_completionEmitted = true;
                emit runningChanged();
                emit completed(-1, QProcess::CrashExit, m_output, false);
            }
        }
    });
}

bool ProcessRunner::start(const QString& program, const QStringList& arguments) {
    if (running() || program.isEmpty()) {
        return false;
    }
    m_output.clear();
    m_cancelRequested = false;
    m_completionEmitted = false;
    m_process.setProgram(program);
    m_process.setArguments(arguments);
    qInfo().noquote() << "Starting process:" << program << arguments.join(' ');
    m_process.start();
    return true;
}

void ProcessRunner::cancel() {
    if (!running()) {
        return;
    }
    m_cancelRequested = true;
    m_process.terminate();
    QTimer::singleShot(2500, this, [this] {
        if (running()) {
            m_process.kill();
        }
    });
}

void ProcessRunner::writeInput(const QString& text) {
    if (running()) {
        m_process.write(text.toUtf8());
    }
}

void ProcessRunner::readOutput() {
    appendOutput(m_process.readAllStandardOutput(), false);
}

void ProcessRunner::readError() {
    appendOutput(m_process.readAllStandardError(), true);
}

void ProcessRunner::processFinished(int exitCode, QProcess::ExitStatus status) {
    if (m_completionEmitted) {
        return;
    }
    m_completionEmitted = true;
    readOutput();
    readError();
    const bool cancelled = m_cancelRequested;
    emit runningChanged();
    emit completed(exitCode, status, m_output, cancelled);
    qInfo().noquote() << "Process finished:" << exitCode << (status == QProcess::NormalExit ? "normal" : "crashed") << (cancelled ? "cancelled" : "completed");
    m_cancelRequested = false;
}

void ProcessRunner::appendOutput(const QByteArray& bytes, bool error) {
    if (bytes.isEmpty()) {
        return;
    }
    const QString text = QString::fromLocal8Bit(bytes);
    m_output += text;
    if (error) {
        emit errorAvailable(text);
    } else {
        emit outputAvailable(text);
    }
}
