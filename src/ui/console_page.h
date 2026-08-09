#pragma once

#include <QWidget>

class QLineEdit;
class QPlainTextEdit;
class QPushButton;

/// Console page (reference "Console Output" tab): streams process output and
/// forwards typed input to interactive processes (PTY via socat).
class ConsolePage : public QWidget {
    Q_OBJECT

public:
    explicit ConsolePage(QWidget* parent = nullptr);

    void appendOutput(const QString& text);
    void appendError(const QString& text);
    void clear();
    void setRunning(bool running);
    void setTitle(const QString& text);

signals:
    void inputSubmitted(const QByteArray& data);
    void cancelRequested();

private:
    QPlainTextEdit* m_output = nullptr;
    QLineEdit* m_input = nullptr;
    QPushButton* m_cancel = nullptr;
    QPushButton* m_stop = nullptr;
};
