#pragma once
#include "Layout.hpp"
#include <QPainter>
#include <QWidget>
#include <vector>

class KeyboardView : public QWidget {
    Q_OBJECT
public:
    explicit KeyboardView(std::vector<QRectF> keys, QWidget* parent = nullptr)
        : QWidget(parent), keys_(std::move(keys)), colors_(keys_.size(), qRgb(0, 0, 0)) {
        setMinimumHeight(220);
    }
    void setColors(const std::vector<QRgb>& c) { colors_ = c; update(); }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QSizeF ls = bloody::layoutSize(keys_);
        const double margin = 24;
        const double s = std::min((width() - 2 * margin) / ls.width(), (height() - 2 * margin) / ls.height());
        const QPointF origin((width() - ls.width() * s) / 2, (height() - ls.height() * s) / 2);

        // chassis
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(14, 14, 18));
        p.drawRoundedRect(QRectF(origin - QPointF(16, 16), QSizeF(ls.width() * s + 32, ls.height() * s + 32)), 18, 18);

        for (std::size_t i = 0; i < keys_.size(); ++i) {
            const QRectF k = keys_[i];
            const QRectF r(origin.x() + k.x() * s + 2, origin.y() + k.y() * s + 2,
                           k.width() * s - 4, k.height() * s - 4);
            const QColor c = QColor::fromRgb(colors_[i]);
            QColor glow = c; glow.setAlpha(60);
            p.setBrush(glow);
            p.drawRoundedRect(r.adjusted(-3, -3, 3, 3), 8, 8);         // glow
            p.setBrush(QColor(26, 26, 33));
            p.drawRoundedRect(r, 6, 6);                                  // keycap
            p.setBrush(c);
            p.drawRoundedRect(r.adjusted(2, r.height() - 8, -2, -2), 3, 3);  // lit edge
            QColor top = c; top.setAlpha(45);
            p.setBrush(top);
            p.drawRoundedRect(r.adjusted(3, 3, -3, -3), 4, 4);
        }
    }

private:
    std::vector<QRectF> keys_;
    std::vector<QRgb> colors_;
};
