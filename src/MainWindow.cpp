#include "MainWindow.hpp"
#include "EffectEditor.hpp"
#include "Protocol.hpp"
#include "Style.hpp"
#include <QApplication>
#include <QColorDialog>
#include <QComboBox>
#include <QTimer>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QSettings>
#include <QSignalBlocker>
#include <QVBoxLayout>

static QIcon makeIcon() {
    QPixmap pm(64, 64);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#14141b"));
    p.drawRoundedRect(QRectF(2, 14, 60, 36), 8, 8);
    const QColor cols[] = {QColor("#ff3b57"), QColor("#ffb020"), QColor("#2ee6a6"), QColor("#3b82ff")};
    for (int r = 0; r < 2; ++r)
        for (int c = 0; c < 4; ++c) {
            p.setBrush(cols[(r + c) % 4]);
            p.drawRoundedRect(QRectF(8 + c * 13.5, 20 + r * 14, 10, 9), 2, 2);
        }
    return QIcon(pm);
}

MainWindow::MainWindow() : layout_(bloody::buildLayout()) {
    setWindowTitle("Bloody RGB Studio");
    setWindowIcon(makeIcon());
    resize(1100, 780);
    qApp->setStyleSheet(kStyle);

    userEffects_ = bloody::loadUserEffects(bloody::userEffectsPath());
    buildUi();
    buildTray();
    rebuildEffectList("Static");
    screen_ = new ScreenWatcher(this);
    connect(screen_, &ScreenWatcher::changed, this, [this] { updateScreenState(); });
    screenInfo_->setText(screen_->hasMonitorSource()
                             ? "Detecting via: " + screen_->description()
                             : "No monitor-state source found on this desktop (use Test to preview).");
    loadSettings();
    refreshColorButtons();

    clock_.start();
    fpsClock_.start();
    connect(&timer_, &QTimer::timeout, this, &MainWindow::tick);
    timer_.start(1000 / fps_->value());
}

MainWindow::~MainWindow() { saveSettings(); }

// ------------------------------------------------------------------ UI

void MainWindow::buildUi() {
    auto* root = new QWidget; root->setObjectName("root");
    auto* col = new QVBoxLayout(root);
    col->setContentsMargins(20, 16, 20, 20);
    col->setSpacing(14);

    view_ = new KeyboardView(layout_);
    col->addWidget(view_, 3);

    auto* row = new QHBoxLayout; row->setSpacing(14);
    col->addLayout(row, 2);

    // Effects
    auto* effBox = new QGroupBox("Effect");
    auto* effLay = new QVBoxLayout(effBox);
    effects_ = new QListWidget;
    effLay->addWidget(effects_, 1);
    fnHint_ = new QLabel;
    fnHint_->setWordWrap(true);
    fnHint_->setStyleSheet("background:#2a1d22; color:#ffb3c0; border-radius:8px; padding:8px;");
    fnHint_->hide();
    effLay->addWidget(fnHint_);
    auto* btns = new QHBoxLayout;
    newBtn_ = new QPushButton("+ New");
    editBtn_ = new QPushButton("Edit");
    delBtn_ = new QPushButton("Delete");
    btns->addWidget(newBtn_); btns->addWidget(editBtn_); btns->addWidget(delBtn_);
    effLay->addLayout(btns);
    row->addWidget(effBox, 2);
    connect(effects_, &QListWidget::currentRowChanged, this, &MainWindow::onEffectSelected);
    connect(effects_, &QListWidget::itemDoubleClicked, this, [this] { editEffect(); });
    connect(newBtn_, &QPushButton::clicked, this, &MainWindow::newEffect);
    connect(editBtn_, &QPushButton::clicked, this, &MainWindow::editEffect);
    connect(delBtn_, &QPushButton::clicked, this, &MainWindow::deleteEffect);

    // Parameters
    auto* parBox = new QGroupBox("Parameters");
    auto* form = new QFormLayout(parBox);
    primaryBtn_ = new QPushButton; secondaryBtn_ = new QPushButton;
    connect(primaryBtn_, &QPushButton::clicked, this, [this] { pickColor(true); });
    connect(secondaryBtn_, &QPushButton::clicked, this, [this] { pickColor(false); });
    speed_ = new QSlider(Qt::Horizontal); speed_->setRange(1, 50); speed_->setValue(10);
    brightness_ = new QSlider(Qt::Horizontal); brightness_->setRange(0, 100); brightness_->setValue(100);
    connect(speed_, &QSlider::valueChanged, this, [this](int v) { params_.speed = v / 10.0; });
    connect(brightness_, &QSlider::valueChanged, this, [this](int v) { params_.brightness = v / 100.0; });
    fps_ = new QSpinBox; fps_->setRange(1, 30); fps_->setValue(10); fps_->setSuffix(" fps");
    connect(fps_, &QSpinBox::valueChanged, this, [this](int v) { timer_.setInterval(1000 / v); });
    fpsLabel_ = new QLabel("measuring…");
    fpsLabel_->setStyleSheet("color:#8a8a9a;");
    form->addRow("Primary color", primaryBtn_);
    form->addRow("Secondary color", secondaryBtn_);
    form->addRow("Speed", speed_);
    form->addRow("Brightness", brightness_);
    form->addRow("Frame rate", fps_);
    form->addRow(fpsLabel_);

    offEffect_ = new QComboBox;
    offEffect_->addItem("Keep current effect", "");
    offOnLock_ = new QCheckBox("Also when the screen is locked");
    offTest_ = new QPushButton("Test (3 s)");
    screenInfo_ = new QLabel;
    screenInfo_->setWordWrap(true);
    screenInfo_->setStyleSheet("color:#8a8a9a;");
    form->addRow("When monitor is off", offEffect_);
    form->addRow(offOnLock_);
    form->addRow(offTest_);
    form->addRow(screenInfo_);
    connect(offEffect_, &QComboBox::currentIndexChanged, this, [this] { updateScreenState(); });
    connect(offOnLock_, &QCheckBox::toggled, this, [this] { updateScreenState(); });
    connect(offTest_, &QPushButton::clicked, this, [this] {
        testingOff_ = true;
        updateScreenState();
        QTimer::singleShot(3000, this, [this] { testingOff_ = false; updateScreenState(); });
    });
    row->addWidget(parBox, 3);

    // Device
    auto* devBox = new QGroupBox("Keyboard");
    auto* dev = new QFormLayout(devBox);
    vid_ = new QLineEdit("0x09DA"); pid_ = new QLineEdit("0xFA10");
    uniform_ = new QCheckBox("Whole keyboard (safe mode)"); uniform_->setChecked(true);
    handshake_ = new QCheckBox("Full handshake every frame"); handshake_->setChecked(true);
    apply_ = new QCheckBox("Send to keyboard"); apply_->setChecked(true);
    reset_ = new QCheckBox("Reset device on connect (as Python tool)"); reset_->setChecked(true);
    legacy_ = new QCheckBox("Legacy libusb mode (keys stop working)");
    connect(uniform_, &QCheckBox::toggled, this, [this](bool on) { params_.uniform = on; });
    connectBtn_ = new QPushButton("Connect"); connectBtn_->setObjectName("accent");
    connect(connectBtn_, &QPushButton::clicked, this, &MainWindow::toggleConnection);
    status_ = new QLabel("Not connected – preview only"); status_->setWordWrap(true);
    dev->addRow("Vendor ID", vid_);
    dev->addRow("Product ID", pid_);
    dev->addRow(uniform_);
    dev->addRow(handshake_);
    dev->addRow(apply_);
    dev->addRow(reset_);
    dev->addRow(legacy_);
    dev->addRow(connectBtn_);
    dev->addRow(status_);
    row->addWidget(devBox, 3);

    setCentralWidget(root);
}

void MainWindow::refreshColorButtons() {
    auto paint = [](QPushButton* b, const QColor& c) {
        b->setText(c.name().toUpper());
        b->setStyleSheet(QString("background:%1; color:%2; font-weight:600;")
                             .arg(c.name(), c.lightness() > 140 ? "#000" : "#fff"));
    };
    paint(primaryBtn_, params_.primary);
    paint(secondaryBtn_, params_.secondary);
}

void MainWindow::pickColor(bool primary) {
    QColor& target = primary ? params_.primary : params_.secondary;
    const QColor c = QColorDialog::getColor(target, this, "Pick a color");
    if (c.isValid()) { target = c; refreshColorButtons(); }
}

void MainWindow::setStatus(const QString& text, bool error) {
    status_->setText(text);
    status_->setStyleSheet(error ? "color:#ff6b6b;" : "color:#7be495;");
}

// ------------------------------------------------------------------ effects

void MainWindow::rebuildEffectList(const QString& selectKey) {
    QStringList labels, keys;
    {
        QSignalBlocker block(effects_);
        effects_->clear();
        auto add = [&](const QString& text, const QString& key, const QString& plain) {
            auto* it = new QListWidgetItem(text, effects_);
            it->setData(Qt::UserRole, key);
            labels << plain;
            keys << key;
        };
        for (int i = 0; i < kBuiltinCount; ++i) {
            const QString n = bloody::effectName(bloody::EffectType(i));
            add(n, n, n);
        }
        for (const auto& fx : userEffects_) {
            QString label = "★ " + fx.name;
            if (fx.fnSlot > 0) label += QString("    Fn+%1").arg(fx.fnSlot);
            add(label, "user:" + fx.name, "★ " + fx.name);
        }
        int row = 0;
        for (int i = 0; i < effects_->count(); ++i)
            if (effects_->item(i)->data(Qt::UserRole).toString() == selectKey) row = i;
        effects_->setCurrentRow(row);
    }
    {
        QSignalBlocker block(offEffect_);
        const QString prev = offEffect_->currentData().toString();
        offEffect_->clear();
        offEffect_->addItem("Keep current effect", "");
        offEffect_->addItem("Lights off", "off");
        for (int i = 0; i < keys.size(); ++i) offEffect_->addItem(labels[i], keys[i]);
        const int idx = offEffect_->findData(prev);
        offEffect_->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    rebuildTrayEffects();
    onEffectSelected(effects_->currentRow());
}

void MainWindow::rebuildTrayEffects() {
    if (!trayMenu_) return;
    for (QAction* a : effectActions_) { trayMenu_->removeAction(a); a->deleteLater(); }
    effectActions_.clear();
    for (int i = 0; i < effects_->count(); ++i) {
        auto* a = new QAction(effects_->item(i)->text().simplified(), trayMenu_);
        a->setCheckable(true);
        effectGroup_->addAction(a);
        trayMenu_->insertAction(effectsEnd_, a);
        effectActions_.push_back(a);
        connect(a, &QAction::triggered, this, [this, i] { effects_->setCurrentRow(i); });
    }
}

void MainWindow::onEffectSelected(int row) {
    if (row < 0) return;
    const bool isUser = row >= kBuiltinCount;
    if (isUser && row - kBuiltinCount >= userEffects_.size()) return;

    editBtn_->setEnabled(isUser);
    delBtn_->setEnabled(isUser);
    fnHint_->hide();

    if (isUser) {
        const bloody::UserEffect& fx = userEffects_[row - kBuiltinCount];
        if (fx.fnSlot > 0) {
            const QString msg = QString("⌨  Reminder: press Fn+%1 on the keyboard for this effect "
                                        "(the app cannot switch slots itself).").arg(fx.fnSlot);
            fnHint_->setText(msg);
            fnHint_->show();
            if (tray_ && !isVisible())
                tray_->showMessage("Bloody RGB Studio", msg, QSystemTrayIcon::Information, 4000);
        }
    }
    if (row < effectActions_.size()) effectActions_[row]->setChecked(true);
    updateScreenState();   // applies the selection unless a monitor-off effect is active
}

// Engine only: which effect the keyboard is actually driven with.
void MainWindow::applyEffectRow(int row) {
    const bool isUser = row >= kBuiltinCount;
    activeUser_ = isUser ? row - kBuiltinCount : -1;
    lastEffectError_.clear();
    if (isUser) userRenderer_.setEffect(userEffects_[activeUser_]);
    else params_.type = bloody::EffectType(row);
}

void MainWindow::updateScreenState() {
    int row = effects_->currentRow();
    if (row < 0) return;

    const bool off = testingOff_ ||
                     (screen_ && (screen_->monitorOff() || (offOnLock_->isChecked() && screen_->locked())));
    const QString key = offEffect_->currentData().toString();
    lightsOff_ = false;
    if (off && !key.isEmpty()) {
        if (key == "off") {
            lightsOff_ = true;
        } else {
            for (int i = 0; i < effects_->count(); ++i)
                if (effects_->item(i)->data(Qt::UserRole).toString() == key) { row = i; break; }
        }
    }
    applyEffectRow(row);
}

QString MainWindow::uniqueName(const QString& name, int skipIndex) const {
    QString candidate = name;
    for (int n = 2;; ++n) {
        bool clash = false;
        for (int i = 0; i < userEffects_.size(); ++i)
            if (i != skipIndex && userEffects_[i].name == candidate) clash = true;
        if (!clash) return candidate;
        candidate = QString("%1 %2").arg(name).arg(n);
    }
}

void MainWindow::persistUserEffects() {
    if (!bloody::saveUserEffects(bloody::userEffectsPath(), userEffects_))
        setStatus("Could not save " + bloody::userEffectsPath(), true);
}

void MainWindow::newEffect() {
    bloody::UserEffect fx;
    fx.script = bloody::defaultScript();
    EffectEditor dlg(fx, layout_, this);
    if (dlg.exec() != QDialog::Accepted) return;
    fx = dlg.result();
    fx.name = uniqueName(fx.name, -1);
    userEffects_.push_back(fx);
    persistUserEffects();
    rebuildEffectList("user:" + fx.name);
}

void MainWindow::editEffect() {
    const int idx = effects_->currentRow() - kBuiltinCount;
    if (idx < 0 || idx >= userEffects_.size()) return;
    EffectEditor dlg(userEffects_[idx], layout_, this);
    if (dlg.exec() != QDialog::Accepted) return;
    bloody::UserEffect fx = dlg.result();
    fx.name = uniqueName(fx.name, idx);
    userEffects_[idx] = fx;
    persistUserEffects();
    rebuildEffectList("user:" + fx.name);
}

void MainWindow::deleteEffect() {
    const int idx = effects_->currentRow() - kBuiltinCount;
    if (idx < 0 || idx >= userEffects_.size()) return;
    if (QMessageBox::question(this, "Delete effect", QString("Delete “%1”?").arg(userEffects_[idx].name))
        != QMessageBox::Yes)
        return;
    userEffects_.removeAt(idx);
    persistUserEffects();
    rebuildEffectList("Static");
}

// ------------------------------------------------------------------ device

void MainWindow::toggleConnection() {
    if (device_.isOpen()) {
        device_.close();
        connectBtn_->setText("Connect");
        setStatus("Disconnected – preview only");
        return;
    }
    bool okV = false, okP = false;
    const auto vid = vid_->text().toUInt(&okV, 0);
    const auto pid = pid_->text().toUInt(&okP, 0);
    if (!okV || !okP) { setStatus("Invalid vendor/product ID", true); return; }

    setStatus("Connecting…");
    QApplication::processEvents();
    QString err;
    if (!device_.open(std::uint16_t(vid), std::uint16_t(pid), !legacy_->isChecked(), reset_->isChecked(), &err)) {
        setStatus(err, true);
        return;
    }
    if (!handshake_->isChecked())   // otherwise the handshake is sent with every update
        for (const auto& pkt : bloody::initSequence())
            if (!device_.send(pkt, &err)) { setStatus(err, true); device_.close(); return; }
    lastSent_.clear();
    connectBtn_->setText("Disconnect");
    setStatus("Connected via " + device_.backend() + ". If nothing changes, press Fn+1 on the keyboard.");
}

void MainWindow::sendFrame(const std::vector<QRgb>& colors) {
    // The keyboard has 116 LED slots, the preview 104 keys: unused slots repeat the first key.
    std::vector<QRgb> leds(bloody::kLedCount, colors.empty() ? qRgb(0, 0, 0) : colors.front());
    if (!params_.uniform)
        std::copy_n(colors.begin(), std::min<std::size_t>(colors.size(), leds.size()), leds.begin());

    if (leds == lastSent_) return;   // nothing changed (e.g. Static effect)
    lastSent_ = leds;
    ++sends_;

    std::vector<QByteArray> packets;
    if (handshake_->isChecked()) packets = bloody::initSequence();
    for (auto& p : bloody::colorPackets(leds)) packets.push_back(std::move(p));
    packets.push_back(bloody::finalPacket());

    QString err;
    for (const auto& pkt : packets) {
        if (!device_.send(pkt, &err)) {
            device_.close();
            connectBtn_->setText("Connect");
            setStatus("USB error: " + err, true);
            return;
        }
    }
}

void MainWindow::tick() {
    const double t = clock_.elapsed() / 1000.0;
    std::vector<QRgb> colors;
    if (lightsOff_) {
        colors.assign(layout_.size(), qRgb(0, 0, 0));
    } else if (activeUser_ >= 0) {
        colors = userRenderer_.render(t * params_.speed, layout_, params_.uniform, params_.brightness);
        const QString e = userRenderer_.error();
        if (e != lastEffectError_) {
            lastEffectError_ = e;
            if (!e.isEmpty()) setStatus("Effect error: " + e, true);
        }
    } else {
        colors = bloody::renderEffect(params_, t, layout_);
    }
    view_->setColors(colors);
    ++frames_;
    if (device_.isOpen() && apply_->isChecked()) sendFrame(colors);

    if (fpsClock_.elapsed() >= 1000) {
        const double s = fpsClock_.restart() / 1000.0;
        fpsLabel_->setText(QString("Actual: %1 fps preview · %2 updates/s to keyboard")
                               .arg(frames_ / s, 0, 'f', 1).arg(sends_ / s, 0, 'f', 1));
        frames_ = sends_ = 0;
    }
}

// ------------------------------------------------------------------ tray

void MainWindow::buildTray() {
    if (!QSystemTrayIcon::isSystemTrayAvailable()) return;
    tray_ = new QSystemTrayIcon(makeIcon(), this);
    tray_->setToolTip("Bloody RGB Studio");

    auto toggleWindow = [this] {
        if (isVisible()) { hide(); }
        else { show(); raise(); activateWindow(); }
    };

    trayMenu_ = new QMenu(this);
    connect(trayMenu_->addAction("Show / Hide"), &QAction::triggered, this, toggleWindow);
    trayMenu_->addSeparator();
    effectGroup_ = new QActionGroup(this);
    effectsEnd_ = trayMenu_->addSeparator();   // effect actions are inserted before this

    auto* send = trayMenu_->addAction("Send to keyboard");
    send->setCheckable(true);
    send->setChecked(apply_->isChecked());
    connect(send, &QAction::toggled, apply_, &QCheckBox::setChecked);
    connect(apply_, &QCheckBox::toggled, send, &QAction::setChecked);
    trayMenu_->addSeparator();
    connect(trayMenu_->addAction("Quit"), &QAction::triggered, this, [this] { quitting_ = true; qApp->quit(); });

    tray_->setContextMenu(trayMenu_);
    connect(tray_, &QSystemTrayIcon::activated, this, [toggleWindow](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger) toggleWindow();
    });
    tray_->show();
}

void MainWindow::closeEvent(QCloseEvent* e) {
    if (tray_ && tray_->isVisible() && !quitting_) {
        hide();
        e->ignore();
        if (!trayHintShown_) {
            tray_->showMessage("Bloody RGB Studio", "Still running in the tray. Use the tray menu to quit.",
                               QSystemTrayIcon::Information, 3000);
            trayHintShown_ = true;
        }
        return;
    }
    e->accept();
}

// ------------------------------------------------------------------ settings

void MainWindow::loadSettings() {
    QSettings s("bloody-rgb", "bloody-rgb");
    params_.primary = QColor(s.value("primary", "#ff0028").toString());
    params_.secondary = QColor(s.value("secondary", "#0078ff").toString());
    speed_->setValue(s.value("speed", 10).toInt());
    brightness_->setValue(s.value("brightness", 100).toInt());
    uniform_->setChecked(s.value("uniform", true).toBool());
    fps_->setValue(s.value("fps", 10).toInt());
    vid_->setText(s.value("vid", "0x09DA").toString());
    pid_->setText(s.value("pid", "0xFA10").toString());

    offOnLock_->setChecked(s.value("offOnLock", false).toBool());
    const int oi = offEffect_->findData(s.value("offEffect", "").toString());
    if (oi >= 0) offEffect_->setCurrentIndex(oi);

    const QString key = s.value("effect", "Static").toString();
    for (int i = 0; i < effects_->count(); ++i)
        if (effects_->item(i)->data(Qt::UserRole).toString() == key) { effects_->setCurrentRow(i); break; }
}

void MainWindow::saveSettings() {
    QSettings s("bloody-rgb", "bloody-rgb");
    s.setValue("primary", params_.primary.name());
    s.setValue("secondary", params_.secondary.name());
    s.setValue("speed", speed_->value());
    s.setValue("brightness", brightness_->value());
    s.setValue("uniform", uniform_->isChecked());
    s.setValue("fps", fps_->value());
    s.setValue("vid", vid_->text());
    s.setValue("pid", pid_->text());
    s.setValue("offEffect", offEffect_->currentData().toString());
    s.setValue("offOnLock", offOnLock_->isChecked());
    if (auto* it = effects_->currentItem()) s.setValue("effect", it->data(Qt::UserRole));
}
