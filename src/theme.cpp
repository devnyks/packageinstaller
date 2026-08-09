#include "theme.h"

#include <QApplication>
#include <QPalette>

namespace {

struct Tokens {
    QColor background;    // window / page background
    QColor surface;       // cards, panels, inputs
    QColor surfaceAlt;    // hover, list hover
    QColor surfaceStrong; // pressed, selected rows
    QColor text;
    QColor textSecondary;
    QColor textDisabled;
    QColor border;
    QColor borderStrong;
    QColor accent;
    QColor accentHover;
    QColor accentPressed;
    QColor accentText;
    QColor danger;
    QColor success;
    QColor selection;  // selection background
};

Tokens lightTokens() {
    return {
        QColor(0xF3, 0xF3, 0xF3),  // background
        QColor(0xFF, 0xFF, 0xFF),  // surface
        QColor(0xF0, 0xF0, 0xF0),  // surfaceAlt
        QColor(0xE5, 0xE5, 0xE5),  // surfaceStrong
        QColor(0x1B, 0x1B, 0x1B),  // text
        QColor(0x61, 0x61, 0x61),  // textSecondary
        QColor(0xA0, 0xA0, 0xA0),  // textDisabled
        QColor(0xE6, 0xE6, 0xE6),  // border
        QColor(0xD0, 0xD0, 0xD0),  // borderStrong
        QColor(0x00, 0x67, 0xC0),  // accent (Fluent blue)
        QColor(0x00, 0x5A, 0x9E),  // accentHover
        QColor(0x00, 0x4E, 0x87),  // accentPressed
        QColor(0xFF, 0xFF, 0xFF),  // accentText
        QColor(0xC4, 0x2B, 0x1C),  // danger
        QColor(0x0F, 0x7B, 0x0F),  // success
        QColor(0xE3, 0xEF, 0xFA),  // selection
    };
}

Tokens darkTokens() {
    return {
        QColor(0x1F, 0x1F, 0x1F),  // background
        QColor(0x2B, 0x2B, 0x2B),  // surface
        QColor(0x33, 0x33, 0x33),  // surfaceAlt
        QColor(0x3D, 0x3D, 0x3D),  // surfaceStrong
        QColor(0xFF, 0xFF, 0xFF),  // text
        QColor(0xC8, 0xC8, 0xC8),  // textSecondary
        QColor(0x6E, 0x6E, 0x6E),  // textDisabled
        QColor(0x3A, 0x3A, 0x3A),  // border
        QColor(0x49, 0x49, 0x49),  // borderStrong
        QColor(0x4C, 0xC2, 0xFF),  // accent
        QColor(0x6D, 0xCF, 0xFF),  // accentHover
        QColor(0x3A, 0xA6, 0xE8),  // accentPressed
        QColor(0x00, 0x00, 0x00),  // accentText
        QColor(0xF1, 0x70, 0x7A),  // danger
        QColor(0x6C, 0xCB, 0x5F),  // success
        QColor(0x1F, 0x3A, 0x4E),  // selection
    };
}

}  // namespace

namespace {

const Tokens& current() {
    static Tokens active = lightTokens();
    return active;
}

Tokens& activeTokens() {
    return const_cast<Tokens&>(current());
}

}  // namespace

namespace theme {

Mode modeFromString(const QString& s) {
    if (s == QLatin1String("light")) {
        return Mode::Light;
    }
    if (s == QLatin1String("dark")) {
        return Mode::Dark;
    }
    return Mode::System;
}

QString modeToString(Mode m) {
    switch (m) {
    case Mode::Light: return QStringLiteral("light");
    case Mode::Dark: return QStringLiteral("dark");
    case Mode::System: break;
    }
    return QStringLiteral("system");
}

QString stylesheet() {
    const auto& t = current();
    const auto c = [](const QColor& col) { return col.name(); };
    // Token mapping (keep in sync with the .arg() list at the bottom):
    //   %1 background   %2 surface      %3 text          %4 textSecondary
    //   %5 border       %6 borderStrong %7 accent        %8 accentHover
    //   %9 accentPressed %10 accentText %11 danger       %12 success
    //   %13 selection   %14 surfaceAlt  %15 surfaceStrong %16 textDisabled
    return QStringLiteral(R"QSS(
QWidget#card {
    background-color: %2;
    border: 1px solid %5;
    border-radius: 10px;
}
QWidget#card[cardHover="true"] { border-color: %7; }
QLabel#cardTitle { font-size: 14px; font-weight: 600; }
QWidget#categoryCard {
    background-color: %2;
    border: 1px solid %5;
    border-radius: 12px;
}
QWidget#categoryCard[cardHover="true"] { border-color: %7; background-color: %13; }
QWidget#detailPanel { background-color: %2; border-left: 1px solid %5; }
QWidget#consoleOutput { background-color: %1; }

QMainWindow, QDialog, #pageRoot {
    background-color: %1;
}
QWidget {
    color: %3;
    font-size: 13px;
}
QLabel#pageTitle {
    font-size: 24px;
    font-weight: 600;
}
QLabel#pageSubtitle {
    color: %4;
}
QLabel#sectionTitle {
    font-size: 16px;
    font-weight: 600;
}
QLabel[status="installed"] { color: %12; font-weight: 600; }
QLabel[status="upgradable"] { color: %7; font-weight: 600; }
QLabel[status="notInstalled"] { color: %4; }

QLineEdit, QComboBox, QSpinBox, QPlainTextEdit, QTextEdit {
    background-color: %2;
    border: 1px solid %5;
    border-radius: 6px;
    padding: 6px 10px;
    selection-background-color: %7;
    selection-color: white;
}
QLineEdit:focus, QComboBox:focus, QPlainTextEdit:focus {
    border: 1px solid %6;
}
QComboBox::drop-down { border: none; width: 24px; }
QComboBox QAbstractItemView {
    background-color: %2;
    border: 1px solid %5;
    selection-background-color: %7;
    selection-color: white;
    outline: none;
}

QPushButton {
    background-color: transparent;
    border: 1px solid %5;
    border-radius: 6px;
    padding: 6px 16px;
    font-weight: 500;
}
QPushButton:hover { background-color: %14; }
QPushButton:pressed { background-color: %15; }
QPushButton:disabled { color: %16; background-color: transparent; }
QPushButton[accent="true"] {
    background-color: %7;
    color: %10;
    border: 1px solid %7;
}
QPushButton[accent="true"]:hover { background-color: %8; border-color: %8; }
QPushButton[accent="true"]:pressed { background-color: %9; border-color: %9; }
QPushButton[accent="true"]:disabled { background-color: %15; color: %16; border-color: %15; }
QPushButton[danger="true"] {
    background-color: transparent;
    color: %11;
    border: 1px solid %11;
}
QPushButton[danger="true"]:hover { background-color: %11; color: %10; }

QPushButton[navItem="true"] {
    border: none;
    border-radius: 8px;
    padding: 0;
    background: transparent;
    text-align: left;
}
QPushButton[navItem="true"]:hover { background-color: %14; }
QPushButton[navItem="true"]:checked { background-color: %13; }
QPushButton[navItem="true"]:checked QLabel { color: %7; font-weight: 600; }
QLabel#navTitle { font-weight: 600; color: %4; }

QToolButton { border: none; border-radius: 6px; padding: 4px; }
QToolButton:hover { background-color: %14; }
QToolButton:pressed { background-color: %15; }
QToolButton:checked { background-color: %14; }
QToolButton:disabled { color: %16; }

QScrollArea { border: none; background: transparent; }
QScrollBar:vertical { background: transparent; width: 10px; margin: 0; }
QScrollBar::handle:vertical { background: %5; border-radius: 5px; min-height: 30px; }
QScrollBar::handle:vertical:hover { background: %4; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 0; }
QScrollBar::handle:horizontal { background: %5; border-radius: 5px; min-width: 30px; }
QScrollBar::handle:horizontal:hover { background: %4; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

QTableView, QTreeView {
    background-color: %2;
    alternate-background-color: %1;
    border: 1px solid %5;
    border-radius: 8px;
    selection-background-color: %13;
    selection-color: %3;
    outline: none;
}
QTableView::item, QTreeView::item { padding: 4px 6px; }
QTableView::item:hover, QTreeView::item:hover { background-color: %14; }
QTableView::item:selected, QTreeView::item:selected { background-color: %13; color: %3; }
QHeaderView::section {
    background-color: %2;
    border: none;
    border-bottom: 1px solid %5;
    padding: 8px 6px;
    font-weight: 600;
}
QTableView::indicator, QTreeView::indicator { width: 16px; height: 16px; }

QTabBar::tab {
    background: transparent;
    padding: 8px 16px;
    border: none;
    color: %4;
}
QTabBar::tab:selected { color: %7; border-bottom: 2px solid %7; }

QCheckBox::indicator { width: 16px; height: 16px; }

QMenu {
    background-color: %2;
    border: 1px solid %5;
    border-radius: 8px;
    padding: 4px;
}
QMenu::item { padding: 6px 24px 6px 12px; border-radius: 4px; }
QMenu::item:selected { background-color: %14; }
QMenu::separator { height: 1px; background: %5; margin: 4px 8px; }

QToolTip {
    background-color: %2;
    color: %3;
    border: 1px solid %5;
    border-radius: 4px;
    padding: 4px 8px;
}

QMessageBox { background-color: %1; }
QProgressBar {
    border: 1px solid %5;
    border-radius: 4px;
    background-color: %1;
    text-align: center;
    color: %3;
}
QProgressBar::chunk { background-color: %7; border-radius: 3px; }

QSplitter::handle { background-color: %5; width: 1px; }
)QSS")
        .arg(c(t.background), c(t.surface), c(t.text), c(t.textSecondary),
            c(t.border), c(t.borderStrong), c(t.accent), c(t.accentHover),
            c(t.accentPressed), c(t.accentText), c(t.danger), c(t.success),
            c(t.selection), c(t.surfaceAlt), c(t.surfaceStrong), c(t.textDisabled));
}

void apply(Mode mode) {
    Tokens t;
    if (mode == Mode::Dark) {
        t = darkTokens();
    } else if (mode == Mode::Light) {
        t = lightTokens();
    } else {
        // follow the platform palette (captured once, so switching back to
        // "System" from a manual theme returns to the real platform theme)
        static const bool platformDark =
            QApplication::palette().color(QPalette::Window).lightness() < 128;
        t = platformDark ? darkTokens() : lightTokens();
    }

    activeTokens() = t;

    QPalette pal = QApplication::palette();
    pal.setColor(QPalette::Window, t.background);
    pal.setColor(QPalette::WindowText, t.text);
    pal.setColor(QPalette::Base, t.surface);
    pal.setColor(QPalette::AlternateBase, t.surfaceAlt);
    pal.setColor(QPalette::Text, t.text);
    pal.setColor(QPalette::PlaceholderText, t.textDisabled);
    pal.setColor(QPalette::Button, t.surface);
    pal.setColor(QPalette::ButtonText, t.text);
    pal.setColor(QPalette::Highlight, t.accent);
    pal.setColor(QPalette::HighlightedText, t.accentText);
    pal.setColor(QPalette::ToolTipBase, t.surface);
    pal.setColor(QPalette::ToolTipText, t.text);
    pal.setColor(QPalette::Link, t.accent);
    pal.setColor(QPalette::Disabled, QPalette::Text, t.textDisabled);
    pal.setColor(QPalette::Disabled, QPalette::WindowText, t.textDisabled);
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, t.textDisabled);
    if (t.background.lightness() < 128) {
        pal.setColor(QPalette::Light, t.surfaceAlt);
        pal.setColor(QPalette::Midlight, t.surfaceAlt);
        pal.setColor(QPalette::Mid, t.surfaceStrong);
        pal.setColor(QPalette::Dark, t.borderStrong);
        pal.setColor(QPalette::Shadow, t.border);
    }
    QApplication::setPalette(pal);
    qApp->setStyleSheet(stylesheet());
}

}  // namespace theme
