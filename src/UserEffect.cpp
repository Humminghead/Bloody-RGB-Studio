#include "UserEffect.hpp"
#include "Effects.hpp"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace bloody {

QString defaultScript() {
    return QStringLiteral(R"JS(// Called for every key, every frame.
//   x, y : key position, 0..1 (left -> right, top -> bottom)
//   t    : time in seconds (the Speed slider is already applied)
//   i    : key index
// Return [r, g, b] (0-255) or a "#rrggbb" string.
function color(x, y, t, i) {
  var h = (x * 0.6 + t * 0.1) % 1;
  var r = Math.sin(h * 6.283) * 0.5 + 0.5;
  var g = Math.sin((h + 0.33) * 6.283) * 0.5 + 0.5;
  var b = Math.sin((h + 0.66) * 6.283) * 0.5 + 0.5;
  return [r * 255, g * 255, b * 255];
}
)JS");
}

static QList<UserEffect> presets() {
    UserEffect ocean;
    ocean.name = "Ocean"; ocean.fnSlot = 2;
    ocean.stops = {QColor("#00c6ff"), QColor("#0072ff"), QColor("#00ffa3")};

    UserEffect sunset;
    sunset.name = "Sunset"; sunset.fnSlot = 3; sunset.mode = GradientMode::PingPong;
    sunset.stops = {QColor("#ff512f"), QColor("#f09819"), QColor("#dd2476")};

    UserEffect scanner;
    scanner.name = "Scanner"; scanner.fnSlot = 4; scanner.kind = UserKind::Script;
    scanner.script = QStringLiteral(R"JS(// A red bar sweeping left and right
function color(x, y, t, i) {
  var pos = Math.abs(((t * 0.4) % 2) - 1);
  var v = Math.max(0, 1 - Math.abs(x - pos) * 6);
  return [255 * v, 20 * v, 40 * v];
}
)JS");
    return {ocean, sunset, scanner};
}

QString userEffectsPath() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    return dir + "/effects.json";
}

QList<UserEffect> loadUserEffects(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return presets();
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return presets();

    QList<UserEffect> out;
    for (const QJsonValue& v : doc.object().value("effects").toArray()) {
        const QJsonObject o = v.toObject();
        UserEffect e;
        e.name = o.value("name").toString("My effect");
        e.fnSlot = std::clamp(o.value("fn").toInt(), 0, kMaxFnSlot);
        e.kind = o.value("kind").toString() == "script" ? UserKind::Script : UserKind::Gradient;
        e.stops.clear();
        for (const QJsonValue& c : o.value("stops").toArray()) e.stops.push_back(QColor(c.toString()));
        if (e.stops.isEmpty()) e.stops = {QColor("#ff3b57"), QColor("#3b82ff")};
        e.mode = GradientMode(std::clamp(o.value("mode").toInt(), 0, 2));
        e.angle = o.value("angle").toDouble();
        e.script = o.value("script").toString();
        out.push_back(e);
    }
    return out;
}

bool saveUserEffects(const QString& path, const QList<UserEffect>& list) {
    QJsonArray arr;
    for (const UserEffect& e : list) {
        QJsonArray stops;
        for (const QColor& c : e.stops) stops.append(c.name());
        QJsonObject o;
        o["name"] = e.name;
        o["fn"] = e.fnSlot;
        o["kind"] = QString(e.kind == UserKind::Script ? "script" : "gradient");
        o["stops"] = stops;
        o["mode"] = int(e.mode);
        o["angle"] = e.angle;
        o["script"] = e.script;
        arr.append(o);
    }
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(QJsonDocument(QJsonObject{{"effects", arr}}).toJson());
    return true;
}

void UserEffectRenderer::setEffect(const UserEffect& e) {
    fx_ = e;
    error_.clear();
    fn_ = QJSValue();
    if (fx_.kind != UserKind::Script) return;

    engine_.globalObject().deleteProperty("color");
    const QJSValue r = engine_.evaluate(fx_.script);
    if (r.isError()) {
        error_ = QString("Line %1: %2").arg(r.property("lineNumber").toInt()).arg(r.toString());
        return;
    }
    fn_ = engine_.globalObject().property("color");
    if (!fn_.isCallable()) error_ = "Script must define: function color(x, y, t, i)";
}

QRgb UserEffectRenderer::sampleGradient(double p, bool cyclic) const {
    const int n = fx_.stops.size();
    if (n == 0) return qRgb(0, 0, 0);
    if (n == 1) return fx_.stops[0].rgb();
    double pos = p * (cyclic ? n : n - 1);
    int k = int(std::floor(pos));
    const double m = pos - k;
    k = cyclic ? k % n : std::min(k, n - 2);
    const QColor& a = fx_.stops[k];
    const QColor& b = fx_.stops[cyclic ? (k + 1) % n : k + 1];
    return detail::mix(a, b, m).rgb();
}

std::vector<QRgb> UserEffectRenderer::render(double t, const std::vector<QRectF>& keys,
                                             bool uniform, double brightness) {
    std::vector<QRgb> out;
    out.reserve(keys.size());
    const QSizeF size = layoutSize(keys);
    const double a = fx_.angle * std::numbers::pi / 180.0;
    const double ca = std::cos(a), sa = std::sin(a);
    auto frac = [](double v) { return v - std::floor(v); };
    auto ch = [](const QJSValue& v) { return std::clamp(int(v.toNumber() + 0.5), 0, 255); };

    for (std::size_t i = 0; i < keys.size(); ++i) {
        const double nx = uniform ? 0.5 : keys[i].center().x() / size.width();
        const double ny = uniform ? 0.5 : keys[i].center().y() / size.height();
        QRgb c = qRgb(0, 0, 0);

        if (fx_.kind == UserKind::Gradient) {
            const double proj = 0.5 + (nx - 0.5) * ca + (ny - 0.5) * sa;
            switch (fx_.mode) {
                case GradientMode::Wave:
                    c = sampleGradient(frac(proj - 0.12 * t), true);
                    break;
                case GradientMode::Cycle:
                    c = sampleGradient(frac(0.12 * t), true);
                    break;
                case GradientMode::PingPong: {
                    const double ph = frac(0.1 * t) * 2.0;
                    c = sampleGradient(ph < 1.0 ? ph : 2.0 - ph, false);
                    break;
                }
            }
        } else if (error_.isEmpty() && fn_.isCallable()) {
            const QJSValue r = fn_.call(QJSValueList{nx, ny, t, double(i)});
            if (r.isError()) {
                error_ = QString("Runtime error: %1").arg(r.toString());
            } else if (r.isArray()) {
                c = qRgb(ch(r.property(0)), ch(r.property(1)), ch(r.property(2)));
            } else if (r.isString()) {
                const QColor q(r.toString());
                if (q.isValid()) c = q.rgb();
            }
        }
        out.push_back(qRgb(int(qRed(c) * brightness + 0.5), int(qGreen(c) * brightness + 0.5),
                           int(qBlue(c) * brightness + 0.5)));
    }
    return out;
}

}  // namespace bloody
