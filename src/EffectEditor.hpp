#pragma once
#include "KeyboardView.hpp"
#include "UserEffect.hpp"
#include <QComboBox>
#include <QDialog>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTimer>

class EffectEditor : public QDialog {
    Q_OBJECT
public:
    EffectEditor(bloody::UserEffect initial, std::vector<QRectF> keys, QWidget* parent = nullptr);
    bloody::UserEffect result() const { return fx_; }

private:
    void addStop(const QColor& c);
    void syncFromUi();
    void tick();
    void trySave();

    bloody::UserEffect fx_;
    std::vector<QRectF> keys_;
    bloody::UserEffectRenderer renderer_;
    QElapsedTimer clock_;
    QTimer timer_;

    KeyboardView* view_ = nullptr;
    QLineEdit* name_ = nullptr;
    QComboBox *fn_ = nullptr, *kind_ = nullptr, *mode_ = nullptr;
    QSpinBox* angle_ = nullptr;
    QListWidget* stops_ = nullptr;
    QPlainTextEdit* script_ = nullptr;
    QLabel* error_ = nullptr;
    QStackedWidget* pages_ = nullptr;
};
