#pragma once

#include "flatpak.h"

#include <QDialog>

class QComboBox;
class QLineEdit;

/// Flatpak remote manager (reference `ManageRemotes`): list/add/remove
/// remotes and install an app from a flatpakref URL or path. Remote changes
/// are reported via `changed`; a requested flatpakref install is surfaced
/// through `installRef` / `userScope`.
class RemotesDialog : public QDialog {
    Q_OBJECT

public:
    explicit RemotesDialog(FlatpakBackend* backend, QWidget* parent = nullptr);

    bool isChanged() const { return m_changed; }
    QString installRef() const { return m_installRef; }
    bool userScope() const { return m_userScope; }

private:
    void listRemotes();
    void addRemote();
    void removeRemote();
    void requestInstall();

    FlatpakBackend* m_backend = nullptr;
    QComboBox* m_scope = nullptr;
    QComboBox* m_remote = nullptr;
    QLineEdit* m_addUrl = nullptr;
    QLineEdit* m_installRefEdit = nullptr;
    bool m_changed = false;
    bool m_userScope = false;
    QString m_installRef;
};
