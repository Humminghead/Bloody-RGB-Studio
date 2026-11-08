#pragma once
inline constexpr const char* kStyle = R"CSS(
* { font-family: "Inter", "Noto Sans", sans-serif; font-size: 13px; color: #e8e8ee; }
QMainWindow, QWidget#root, QDialog, QMessageBox { background: #0b0b0f; }
QGroupBox { background: #14141b; border: 1px solid #23232e; border-radius: 12px;
            margin-top: 14px; padding: 14px 12px 10px 12px; }
QGroupBox::title { subcontrol-origin: margin; left: 14px; padding: 0 6px; color: #ff3b57; font-weight: 600; }
QListWidget { background: transparent; border: none; outline: none; }
QListWidget::item { background: #1b1b25; border-radius: 8px; padding: 9px 12px; margin: 2px 0; }
QListWidget::item:selected { background: #ff3b57; color: white; }
QListWidget::item:hover:!selected { background: #242433; }
QPushButton { background: #1f1f2b; border: 1px solid #2c2c3b; border-radius: 8px; padding: 7px 14px; }
QPushButton:hover { border-color: #ff3b57; }
QPushButton:disabled { color: #5a5a6a; }
QPushButton#accent { background: #ff3b57; border: none; font-weight: 600; color: white; }
QLineEdit, QSpinBox, QComboBox { background: #1b1b25; border: 1px solid #2c2c3b; border-radius: 6px; padding: 5px 8px; }
QComboBox QAbstractItemView { background: #1b1b25; selection-background-color: #ff3b57; }
QPlainTextEdit { background: #0f0f15; border: 1px solid #2c2c3b; border-radius: 8px; padding: 6px; }
QMenu { background: #14141b; border: 1px solid #2c2c3b; padding: 4px; }
QMenu::item { padding: 6px 18px; border-radius: 4px; }
QMenu::item:selected { background: #ff3b57; }
QSlider::groove:horizontal { height: 4px; background: #2c2c3b; border-radius: 2px; }
QSlider::sub-page:horizontal { background: #ff3b57; border-radius: 2px; }
QSlider::handle:horizontal { background: white; width: 14px; margin: -6px 0; border-radius: 7px; }
QCheckBox::indicator { width: 16px; height: 16px; border-radius: 4px; border: 1px solid #3a3a4d; background: #1b1b25; }
QCheckBox::indicator:checked { background: #ff3b57; border-color: #ff3b57; }
)CSS";
