#pragma once
#include "Effects.hpp"
#include "KeyboardView.hpp"
#include "ScreenWatcher.hpp"
#include "UsbDevice.hpp"
#include "UserEffect.hpp"
#include <QAction>
#include <QActionGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QSystemTrayIcon>
#include <QTimer>

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* e) override;

private:
    static constexpr int kBuiltinCount = int(bloody::EffectType::Twinkle) + 1;

    void buildUi();
    void buildTray();
    void rebuildEffectList(const QString& selectKey);
    void rebuildTrayEffects();
    void onEffectSelected(int row);
    void applyEffectRow(int row);
    void updateScreenState();
    void newEffect();
    void editEffect();
    void deleteEffect();
    QString uniqueName(const QString& name, int skipIndex) const;
    void persistUserEffects();

    void tick();
    void toggleConnection();
    void sendFrame(const std::vector<QRgb>& colors);
    void pickColor(bool primary);
    void refreshColorButtons();
    void setStatus(const QString& text, bool error = false);
    void loadSettings();
    void saveSettings();

    std::vector<QRectF> layout_;
    bloody::EffectParams params_;
    QList<bloody::UserEffect> userEffects_;
    bloody::UserEffectRenderer userRenderer_;
    int activeUser_ = -1;
    bool lightsOff_ = false, testingOff_ = false;
    ScreenWatcher* screen_ = nullptr;
    QString lastEffectError_;

    UsbDevice device_;
    std::vector<QRgb> lastSent_;
    QElapsedTimer clock_, fpsClock_;
    QTimer timer_;
    int frames_ = 0, sends_ = 0;

    KeyboardView* view_ = nullptr;
    QListWidget* effects_ = nullptr;
    QLabel *fnHint_ = nullptr, *status_ = nullptr, *fpsLabel_ = nullptr;
    QPushButton *primaryBtn_ = nullptr, *secondaryBtn_ = nullptr, *connectBtn_ = nullptr;
    QPushButton *newBtn_ = nullptr, *editBtn_ = nullptr, *delBtn_ = nullptr;
    QSlider *speed_ = nullptr, *brightness_ = nullptr;
    QCheckBox *uniform_ = nullptr, *handshake_ = nullptr, *apply_ = nullptr, *legacy_ = nullptr, *reset_ = nullptr;
    QSpinBox* fps_ = nullptr;
    QComboBox* offEffect_ = nullptr;
    QCheckBox* offOnLock_ = nullptr;
    QPushButton* offTest_ = nullptr;
    QLabel* screenInfo_ = nullptr;
    QLineEdit *vid_ = nullptr, *pid_ = nullptr;

    QSystemTrayIcon* tray_ = nullptr;
    QMenu* trayMenu_ = nullptr;
    QAction* effectsEnd_ = nullptr;
    QActionGroup* effectGroup_ = nullptr;
    QList<QAction*> effectActions_;
    bool quitting_ = false, trayHintShown_ = false;
};
