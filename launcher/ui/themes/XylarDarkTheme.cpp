#include "XylarDarkTheme.h"

#include <QObject>

// Palette (original XylarJava colours)
//   background   #12141B   surface   #1A1D27   raised   #232736
//   text         #E9EBF3   muted     #8E94A8   accent   #FF9A3C
namespace {
const QColor kWindow(0x12, 0x14, 0x1B);
const QColor kBase(0x0E, 0x10, 0x16);
const QColor kSurface(0x1A, 0x1D, 0x27);
const QColor kRaised(0x23, 0x27, 0x36);
const QColor kText(0xE9, 0xEB, 0xF3);
const QColor kMuted(0x8E, 0x94, 0xA8);
const QColor kAccent(0xFF, 0x9A, 0x3C);
const QColor kAccentText(0x1A, 0x10, 0x06);
const QColor kLink(0xFF, 0xB8, 0x6E);
}  // namespace

QString XylarDarkTheme::id()
{
    return "xylar_dark";
}

QString XylarDarkTheme::name()
{
    return QObject::tr("Xylar Dark");
}

QString XylarDarkTheme::tooltip()
{
    return QObject::tr("The default XylarJava look: dark, rounded and roomy.");
}

QPalette XylarDarkTheme::colorScheme()
{
    QPalette p;
    p.setColor(QPalette::Window, kWindow);
    p.setColor(QPalette::WindowText, kText);
    p.setColor(QPalette::Base, kBase);
    p.setColor(QPalette::AlternateBase, kSurface);
    p.setColor(QPalette::ToolTipBase, kRaised);
    p.setColor(QPalette::ToolTipText, kText);
    p.setColor(QPalette::Text, kText);
    p.setColor(QPalette::Button, kSurface);
    p.setColor(QPalette::ButtonText, kText);
    p.setColor(QPalette::BrightText, QColor(0xFF, 0x6B, 0x6B));
    p.setColor(QPalette::Link, kLink);
    p.setColor(QPalette::LinkVisited, kLink);
    p.setColor(QPalette::Highlight, kAccent);
    p.setColor(QPalette::HighlightedText, kAccentText);
    p.setColor(QPalette::PlaceholderText, kMuted);
    p.setColor(QPalette::Light, kRaised.lighter(130));
    p.setColor(QPalette::Midlight, kRaised);
    p.setColor(QPalette::Mid, QColor(0x2C, 0x31, 0x42));
    p.setColor(QPalette::Dark, QColor(0x0A, 0x0B, 0x10));
    p.setColor(QPalette::Shadow, QColor(0, 0, 0));
    return fadeInactive(p, fadeAmount(), fadeColor());
}

double XylarDarkTheme::fadeAmount()
{
    return 0.5;
}

QColor XylarDarkTheme::fadeColor()
{
    return kWindow;
}

bool XylarDarkTheme::hasStyleSheet()
{
    return true;
}

QString XylarDarkTheme::appStyleSheet()
{
    // Kept deliberately general so every dialog and settings page benefits.
    return QStringLiteral(
        "QToolTip { color: #E9EBF3; background-color: #232736; border: 1px solid #2C3142; border-radius: 6px; padding: 4px 6px; }"

        "QPushButton { background-color: #1A1D27; border: 1px solid #2C3142; border-radius: 8px; padding: 6px 14px; }"
        "QPushButton:hover { background-color: #232736; border-color: #3A4057; }"
        "QPushButton:pressed { background-color: #2C3142; }"
        "QPushButton:default { border-color: #FF9A3C; }"
        "QPushButton:disabled { color: #5C6275; background-color: #161922; border-color: #222634; }"

        "QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QPlainTextEdit, QTextEdit {"
        " background-color: #0E1016; border: 1px solid #2C3142; border-radius: 8px; padding: 4px 8px;"
        " selection-background-color: #FF9A3C; selection-color: #1A1006; }"
        "QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus, QPlainTextEdit:focus, QTextEdit:focus {"
        " border-color: #FF9A3C; }"
        "QComboBox::drop-down { border: none; width: 22px; }"
        "QComboBox::down-arrow { image: url(:/xylar/arrow-down.png); width: 10px; height: 10px; }"
        "QSpinBox, QDoubleSpinBox { padding-right: 22px; }"
        "QSpinBox::up-button, QDoubleSpinBox::up-button { subcontrol-origin: border; subcontrol-position: top right;"
        " width: 18px; border: none; background: transparent; margin: 3px 3px 0 0; }"
        "QSpinBox::down-button, QDoubleSpinBox::down-button { subcontrol-origin: border; subcontrol-position: bottom right;"
        " width: 18px; border: none; background: transparent; margin: 0 3px 3px 0; }"
        "QSpinBox::up-button:hover, QDoubleSpinBox::up-button:hover, QSpinBox::down-button:hover,"
        " QDoubleSpinBox::down-button:hover { background: #232736; border-radius: 4px; }"
        "QSpinBox::up-arrow, QDoubleSpinBox::up-arrow { image: url(:/xylar/arrow-up.png); width: 9px; height: 9px; }"
        "QSpinBox::down-arrow, QDoubleSpinBox::down-arrow { image: url(:/xylar/arrow-down.png); width: 9px; height: 9px; }"

        "QCheckBox, QRadioButton { spacing: 8px; }"
        "QCheckBox::indicator, QGroupBox::indicator { width: 16px; height: 16px; border: 1px solid #4A516A;"
        " border-radius: 4px; background-color: #0E1016; }"
        "QRadioButton::indicator { width: 16px; height: 16px; border: 1px solid #4A516A; border-radius: 8px;"
        " background-color: #0E1016; }"
        "QCheckBox::indicator:hover, QGroupBox::indicator:hover, QRadioButton::indicator:hover { border-color: #FF9A3C; }"
        "QCheckBox::indicator:checked, QGroupBox::indicator:checked { background-color: #FF9A3C; border-color: #FF9A3C;"
        " image: url(:/xylar/check.png); }"
        "QCheckBox::indicator:indeterminate { background-color: #3A4057; border-color: #FF9A3C; }"
        "QRadioButton::indicator:checked { background-color: #FF9A3C; border-color: #FF9A3C; image: url(:/xylar/radio.png); }"
        "QCheckBox::indicator:disabled, QRadioButton::indicator:disabled { border-color: #2C3142; background-color: #161922; }"
        "QCheckBox::indicator:checked:disabled, QRadioButton::indicator:checked:disabled { background-color: #5C6275;"
        " border-color: #5C6275; }"
        "QComboBox QAbstractItemView { background-color: #1A1D27; border: 1px solid #2C3142; border-radius: 8px;"
        " selection-background-color: #FF9A3C; selection-color: #1A1006; padding: 4px; }"

        "QMenu { background-color: #1A1D27; border: 1px solid #2C3142; border-radius: 10px; padding: 6px; }"
        "QMenu::item { padding: 6px 22px 6px 12px; border-radius: 6px; }"
        "QMenu::item:selected { background-color: #232736; color: #FFFFFF; }"
        "QMenu::item:disabled { color: #5C6275; }"
        "QMenu::separator { height: 1px; background: #2C3142; margin: 5px 8px; }"
        "QMenuBar { background-color: #12141B; }"
        "QMenuBar::item { padding: 4px 10px; border-radius: 6px; }"
        "QMenuBar::item:selected { background-color: #232736; }"

        "QTabWidget::pane { border: 1px solid #2C3142; border-radius: 10px; top: -1px; }"
        "QTabBar::tab { background: transparent; color: #8E94A8; padding: 7px 14px; border: none;"
        " border-bottom: 2px solid transparent; }"
        "QTabBar::tab:selected { color: #E9EBF3; border-bottom: 2px solid #FF9A3C; }"
        "QTabBar::tab:hover:!selected { color: #E9EBF3; }"

        "QGroupBox { border: 1px solid #2C3142; border-radius: 10px; margin-top: 14px; padding-top: 8px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 4px; color: #8E94A8; }"

        "QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }"
        "QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }"
        "QScrollBar::handle { background: #2C3142; border-radius: 4px; min-height: 24px; min-width: 24px; }"
        "QScrollBar::handle:hover { background: #3A4057; }"
        "QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }"
        "QScrollBar::add-page, QScrollBar::sub-page { background: none; }"

        "QProgressBar { background-color: #0E1016; border: 1px solid #2C3142; border-radius: 6px; text-align: center; }"
        "QProgressBar::chunk { background-color: #FF9A3C; border-radius: 5px; }"

        "QHeaderView::section { background-color: #1A1D27; color: #8E94A8; border: none;"
        " border-bottom: 1px solid #2C3142; padding: 5px 8px; }"

        "QStatusBar { background-color: #0E1016; color: #8E94A8; }"
        "QStatusBar::item { border: none; }");
}
