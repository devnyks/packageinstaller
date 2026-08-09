#pragma once

#include <QDialog>

class QProgressBar;
class QLabel;
class QPushButton;

/// Indeterminate progress dialog with a cancel button (reference
/// `setProgressDialog` semantics: busy indicator while package lists are
/// loaded or processes run; cancellation terminates the running process).
class ProgressOverlay : public QDialog {
    Q_OBJECT

public:
    explicit ProgressOverlay(const QString& label, QWidget* parent = nullptr);

    void setBusy(bool busy);            // enable/disable cancel
    void setStatus(const QString& text);

signals:
    void cancelRequested();

private:
    QLabel* m_status = nullptr;
    QPushButton* m_cancel = nullptr;
};
