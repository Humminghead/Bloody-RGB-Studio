#pragma once
#include "Layout.hpp"
#include <QColor>
#include <QJSEngine>
#include <QJSValue>
#include <QList>
#include <QRgb>
#include <QString>
#include <vector>

namespace bloody {

inline constexpr int kMaxFnSlot = 9;   // keyboard slots offered in the UI: Fn+1 .. Fn+9

enum class UserKind { Gradient = 0, Script = 1 };
enum class GradientMode { Wave = 0, Cycle = 1, PingPong = 2 };

struct UserEffect {
    QString name = "My effect";
    int fnSlot = 0;                       // 0 = none, otherwise "press Fn+<n>"
    UserKind kind = UserKind::Gradient;
    QList<QColor> stops{QColor("#ff3b57"), QColor("#3b82ff")};
    GradientMode mode = GradientMode::Wave;
    double angle = 0.0;                   // degrees, direction of the wave
    QString script;                       // JavaScript, see defaultScript()
};

QString defaultScript();
QString userEffectsPath();
QList<UserEffect> loadUserEffects(const QString& path);   // returns presets if no file yet
bool saveUserEffects(const QString& path, const QList<UserEffect>& list);

class UserEffectRenderer {
public:
    void setEffect(const UserEffect& e);   // (re)compiles the script
    std::vector<QRgb> render(double t, const std::vector<QRectF>& keys, bool uniform, double brightness);
    QString error() const { return error_; }

private:
    QRgb sampleGradient(double p, bool cyclic) const;

    UserEffect fx_;
    QJSEngine engine_;
    QJSValue fn_;
    QString error_;
};

}  // namespace bloody
