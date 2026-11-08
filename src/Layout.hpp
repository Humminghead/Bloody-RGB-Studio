#pragma once
// Full-size ANSI layout (104 keys) in key units. Spec: positive = key width,
// negative = gap, "n*w" = n keys of width w.
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QList>
#include <vector>
#include <algorithm>

namespace bloody {

inline std::vector<QRectF> buildLayout() {
    struct Row { double y; const char* spec; };
    static constexpr Row rows[] = {
        {0.00, "1 -1 4*1 -.5 4*1 -.5 4*1 -.25 3*1"},
        {1.25, "13*1 2 -.25 3*1 -.25 4*1"},
        {2.25, "1.5 12*1 1.5 -.25 3*1 -.25 4*1"},
        {3.25, "1.75 11*1 2.25 -3.5 3*1"},
        {4.25, "2.25 10*1 2.75 -1.25 1 -1.25 4*1"},
        {5.25, "3*1.25 6.25 4*1.25 -.25 3*1 -.25 2 1"},
    };
    std::vector<QRectF> keys;
    for (const auto& row : rows) {
        double x = 0;
        for (const QString& tok : QString(row.spec).split(' ', Qt::SkipEmptyParts)) {
            int n = 1; QString w = tok;
            if (tok.contains('*')) { n = tok.section('*', 0, 0).toInt(); w = tok.section('*', 1); }
            const double v = w.toDouble();
            for (int i = 0; i < n; ++i) {
                if (v < 0) x -= v;
                else { keys.emplace_back(x, row.y, v, 1.0); x += v; }
                if (v < 0) break;
            }
        }
    }
    return keys;
}

inline QSizeF layoutSize(const std::vector<QRectF>& keys) {
    double w = 0, h = 0;
    for (const auto& k : keys) { w = std::max(w, k.right()); h = std::max(h, k.bottom()); }
    return {w, h};
}

}  // namespace bloody
