#pragma once
#include "Layout.hpp"
#include <QColor>
#include <QRgb>
#include <cmath>
#include <numbers>
#include <vector>

namespace bloody {

enum class EffectType { Static, Breathing, ColorCycle, RainbowWave, GradientWave, Twinkle };

inline const char* effectName(EffectType t) {
    switch (t) {
        case EffectType::Static:       return "Static";
        case EffectType::Breathing:    return "Breathing";
        case EffectType::ColorCycle:   return "Color cycle";
        case EffectType::RainbowWave:  return "Rainbow wave";
        case EffectType::GradientWave: return "Gradient wave";
        case EffectType::Twinkle:      return "Twinkle";
    }
    return "";
}

struct EffectParams {
    EffectType type = EffectType::Static;
    QColor primary{255, 0, 40};
    QColor secondary{0, 120, 255};
    double speed = 1.0;        // 0.1 .. 5
    double brightness = 1.0;   // 0 .. 1
    bool uniform = true;       // true: one color for the whole board (safe mode)
};

namespace detail {
inline QColor mix(const QColor& a, const QColor& b, double m) {
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * m,
                            a.greenF() + (b.greenF() - a.greenF()) * m,
                            a.blueF() + (b.blueF() - a.blueF()) * m);
}
inline double hash01(unsigned i) {
    i ^= i >> 16; i *= 0x7feb352dU; i ^= i >> 15; i *= 0x846ca68bU; i ^= i >> 16;
    return (i & 0xffffff) / double(0x1000000);
}
}  // namespace detail

// Stateless: color of every key at time t (seconds).
inline std::vector<QRgb> renderEffect(const EffectParams& p, double t,
                                      const std::vector<QRectF>& keys) {
    constexpr double tau = 2.0 * std::numbers::pi;
    const QSizeF size = layoutSize(keys);
    std::vector<QRgb> out;
    out.reserve(keys.size());

    for (std::size_t i = 0; i < keys.size(); ++i) {
        const double nx = p.uniform ? 0.5 : keys[i].center().x() / size.width();
        const double ny = p.uniform ? 0.5 : keys[i].center().y() / size.height();
        QColor c = p.primary;
        switch (p.type) {
            case EffectType::Static: break;
            case EffectType::Breathing:
                c = detail::mix(QColor(0, 0, 0), p.primary, 0.5 - 0.5 * std::cos(tau * 0.4 * p.speed * t));
                break;
            case EffectType::ColorCycle:
                c = QColor::fromHsvF(std::fmod(0.08 * p.speed * t, 1.0), 1.0, 1.0);
                break;
            case EffectType::RainbowWave: {
                double h = std::fmod(nx * 0.8 + ny * 0.1 - 0.12 * p.speed * t, 1.0);
                if (h < 0) h += 1.0;
                c = QColor::fromHsvF(h, 1.0, 1.0);
                break;
            }
            case EffectType::GradientWave:
                c = detail::mix(p.primary, p.secondary, 0.5 + 0.5 * std::sin(tau * (nx - 0.2 * p.speed * t)));
                break;
            case EffectType::Twinkle: {
                const double ph = detail::hash01(p.uniform ? 0u : unsigned(i));
                const double v = std::pow(0.5 + 0.5 * std::sin(tau * (0.3 * p.speed * t + ph)), 4.0);
                c = detail::mix(p.secondary.darker(300), p.primary, v);
                break;
            }
        }
        out.push_back(qRgb(int(c.red() * p.brightness + 0.5),
                           int(c.green() * p.brightness + 0.5),
                           int(c.blue() * p.brightness + 0.5)));
    }
    return out;
}

}  // namespace bloody
