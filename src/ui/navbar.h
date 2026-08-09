#pragma once

#include <QWidget>

class QButtonGroup;
class QVBoxLayout;
class QPushButton;

/// Left navigation rail (Fluent 2 style). Items are icon+label buttons;
/// the selected item gets an accent-tinted pill background. Items flagged
/// as `bottom` are pinned to the bottom of the rail (e.g. Settings).
class NavBar : public QWidget {
    Q_OBJECT

public:
    explicit NavBar(QWidget* parent = nullptr);

    /// Adds a nav item; returns its id.
    int addItem(const QString& iconName, const QString& text, bool bottom = false);
    void setCurrentIndex(int index);
    int currentIndex() const;
    void setItemEnabled(int index, bool enabled);

signals:
    void itemActivated(int index);

private:
    QButtonGroup* m_group = nullptr;
    QVBoxLayout* m_root = nullptr;
    QList<QPushButton*> m_items;
};
