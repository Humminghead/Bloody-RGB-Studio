#include "EffectEditor.hpp"
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

using namespace bloody;

EffectEditor::EffectEditor(UserEffect initial, std::vector<QRectF> keys, QWidget* parent)
    : QDialog(parent), fx_(std::move(initial)), keys_(std::move(keys)) {
    setWindowTitle("Effect editor");
    resize(900, 720);

    auto* root = new QVBoxLayout(this);
    view_ = new KeyboardView(keys_);
    root->addWidget(view_, 3);

    auto* form = new QFormLayout;
    name_ = new QLineEdit(fx_.name);
    fn_ = new QComboBox;
    fn_->addItem("None");
    for (int n = 1; n <= kMaxFnSlot; ++n) fn_->addItem(QString("Fn+%1").arg(n));
    fn_->setCurrentIndex(fx_.fnSlot);
    kind_ = new QComboBox;
    kind_->addItems({"Gradient animation", "Script (JavaScript)"});
    kind_->setCurrentIndex(int(fx_.kind));
    form->addRow("Name", name_);
    fn_->setToolTip("Reminder only: the app cannot switch the keyboard's Fn slot itself.");
    form->addRow("Fn+ reminder", fn_);
    form->addRow("Type", kind_);
    root->addLayout(form);

    pages_ = new QStackedWidget;
    root->addWidget(pages_, 2);

    // --- gradient page
    auto* gp = new QWidget;
    auto* gl = new QHBoxLayout(gp);
    auto* left = new QVBoxLayout;
    stops_ = new QListWidget;
    stops_->setIconSize(QSize(22, 22));
    stops_->setDragDropMode(QAbstractItemView::InternalMove);
    left->addWidget(new QLabel("Color stops (drag to reorder, double-click to change)"));
    left->addWidget(stops_);
    auto* sb = new QHBoxLayout;
    auto* add = new QPushButton("+ Add color");
    auto* rem = new QPushButton("Remove");
    sb->addWidget(add); sb->addWidget(rem);
    left->addLayout(sb);
    gl->addLayout(left, 2);

    auto* gf = new QFormLayout;
    mode_ = new QComboBox;
    mode_->addItems({"Wave", "Cycle (whole keyboard)", "Ping-pong"});
    mode_->setCurrentIndex(int(fx_.mode));
    angle_ = new QSpinBox; angle_->setRange(0, 359); angle_->setSuffix(" °");
    angle_->setValue(int(fx_.angle));
    gf->addRow("Mode", mode_);
    gf->addRow("Wave direction", angle_);
    gl->addLayout(gf, 1);
    pages_->addWidget(gp);

    // --- script page
    auto* sp = new QWidget;
    auto* sl = new QVBoxLayout(sp);
    script_ = new QPlainTextEdit(fx_.script.isEmpty() ? defaultScript() : fx_.script);
    script_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    error_ = new QLabel;
    error_->setStyleSheet("color:#ff6b6b;");
    error_->setWordWrap(true);
    sl->addWidget(script_);
    sl->addWidget(error_);
    pages_->addWidget(sp);
    pages_->setCurrentIndex(int(fx_.kind));

    auto* note = new QLabel("Tip: position-based effects only look right with \u201cWhole keyboard\u201d "
                            "turned off in the main window (per-key mode is experimental).");
    note->setWordWrap(true);
    note->setStyleSheet("color:#8a8a9a;");
    root->addWidget(note);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &EffectEditor::trySave);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    for (const QColor& c : fx_.stops) addStop(c);

    connect(add, &QPushButton::clicked, this, [this] {
        const QColor c = QColorDialog::getColor(Qt::white, this, "Pick a color");
        if (c.isValid()) { addStop(c); syncFromUi(); renderer_.setEffect(fx_); }
    });
    connect(rem, &QPushButton::clicked, this, [this] {
        if (stops_->count() > 1 && stops_->currentRow() >= 0) {
            delete stops_->takeItem(stops_->currentRow());
            syncFromUi(); renderer_.setEffect(fx_);
        }
    });
    connect(stops_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* it) {
        const QColor c = QColorDialog::getColor(qvariant_cast<QColor>(it->data(Qt::UserRole)), this, "Pick a color");
        if (!c.isValid()) return;
        QPixmap pm(22, 22); pm.fill(c);
        it->setIcon(QIcon(pm));
        it->setText(c.name().toUpper());
        it->setData(Qt::UserRole, c);
        syncFromUi(); renderer_.setEffect(fx_);
    });

    auto changed = [this] { syncFromUi(); renderer_.setEffect(fx_); };
    connect(name_, &QLineEdit::textChanged, this, changed);
    connect(fn_, &QComboBox::currentIndexChanged, this, changed);
    connect(mode_, &QComboBox::currentIndexChanged, this, changed);
    connect(angle_, &QSpinBox::valueChanged, this, changed);
    connect(script_, &QPlainTextEdit::textChanged, this, changed);
    connect(kind_, &QComboBox::currentIndexChanged, this, [this, changed](int k) {
        pages_->setCurrentIndex(k);
        changed();
    });
    connect(stops_->model(), &QAbstractItemModel::rowsMoved, this, changed);

    syncFromUi();
    renderer_.setEffect(fx_);
    clock_.start();
    connect(&timer_, &QTimer::timeout, this, &EffectEditor::tick);
    timer_.start(33);
}

void EffectEditor::addStop(const QColor& c) {
    QPixmap pm(22, 22);
    pm.fill(c);
    auto* it = new QListWidgetItem(QIcon(pm), c.name().toUpper(), stops_);
    it->setData(Qt::UserRole, c);
}

void EffectEditor::syncFromUi() {
    fx_.name = name_->text().trimmed();
    if (fx_.name.isEmpty()) fx_.name = "My effect";
    fx_.fnSlot = fn_->currentIndex();
    fx_.kind = UserKind(kind_->currentIndex());
    fx_.mode = GradientMode(mode_->currentIndex());
    fx_.angle = angle_->value();
    fx_.script = script_->toPlainText();
    fx_.stops.clear();
    for (int i = 0; i < stops_->count(); ++i)
        fx_.stops.push_back(qvariant_cast<QColor>(stops_->item(i)->data(Qt::UserRole)));
}

void EffectEditor::tick() {
    view_->setColors(renderer_.render(clock_.elapsed() / 1000.0, keys_, false, 1.0));
    error_->setText(renderer_.error());
}

void EffectEditor::trySave() {
    syncFromUi();
    renderer_.setEffect(fx_);
    if (fx_.kind == UserKind::Script && !renderer_.error().isEmpty()) {
        QMessageBox::warning(this, "Script error", renderer_.error());
        return;
    }
    accept();
}
