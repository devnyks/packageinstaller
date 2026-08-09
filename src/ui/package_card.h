#pragma once

#include <QCheckBox>
#include <QWidget>

class QLabel;
class QPushButton;

/// A clickable application card for the Discover grid: icon, name, summary,
/// version and a status badge, with an action button (Install / Update /
/// Reinstall / Remove) and a batch-selection checkbox.
class PackageCard : public QWidget {
    Q_OBJECT

public:
    enum class State { NotInstalled, Installed, Upgradable };

    explicit PackageCard(QWidget* parent = nullptr);

    void setPackage(const QString& name, const QString& summary, const QString& version, State state);
    void setIcon(const QIcon& icon);
    void setDescription(const QString& description);

    QString packageName() const { return m_name; }
    State state() const { return m_state; }
    bool isChecked() const;
    void setChecked(bool checked) { m_check->setChecked(checked); }

signals:
    void clicked(const QString& name);
    void actionRequested(const QString& name);
    void checkChanged(const QString& name, bool checked);

protected:
    void mouseReleaseEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void updateActionButton();

    QString m_name;
    QString m_description;
    State m_state = State::NotInstalled;
    QLabel* m_icon = nullptr;
    QLabel* m_nameLabel = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QLabel* m_versionLabel = nullptr;
    QLabel* m_badge = nullptr;
    QPushButton* m_action = nullptr;
    QCheckBox* m_check = nullptr;
    bool m_hover = false;
};
