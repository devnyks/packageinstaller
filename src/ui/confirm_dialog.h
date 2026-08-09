#pragma once

#include <QDialog>

class QLabel;
class QPlainTextEdit;
class QPushButton;

/// Transaction preview / confirmation dialog (reference `confirmActions`):
///  - shows the package list and a summary (download/installed sizes),
///  - "Show Details" reveals the per-package list,
///  - for conflicts, offers Replace / Ignore (replace propagates --noconfirm
///    to the pacman removal step).
/// Returns:
///   Accepted  -> proceed,
///   Rejected  -> cancel (no action),
///   ButtonRole 0 -> conflict "Replace" chosen (sets `replaceConflicts`).
class ConfirmDialog : public QDialog {
    Q_OBJECT

public:
    enum class Mode { Install, Remove };

    explicit ConfirmDialog(Mode mode, const QString& packageSummary,
        const QString& statusText, const QString& detailsText, bool hasConflicts,
        QWidget* parent = nullptr);

    /// True when the user chose "Replace" for conflicting packages.
    bool replaceConflicts() const { return m_replace; }

    static bool confirmQuestion(const QString& title, const QString& text, QWidget* parent);

private:
    bool m_replace = false;
};
