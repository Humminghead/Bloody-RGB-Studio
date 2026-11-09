#pragma once
// Port of rgb_utils.py. Packets are 64 bytes, sent as HID class
// SET_REPORT (bmRequestType 0x21, bRequest 0x09, wValue 0x0307, wIndex 2).
#include <QByteArray>
#include <QRgb>
#include <vector>
#include <iterator>
#include <algorithm>

namespace bloody {

inline constexpr int kLedsPerPacket = 58;
inline constexpr int kLedCount      = kLedsPerPacket * 2;   // 2 packets per channel
inline constexpr int kPacketBytes   = 64;

inline QByteArray packetFromHexPrefix(const char* prefix) {
    return QByteArray::fromHex(QByteArray(prefix).leftJustified(kPacketBytes * 2, '0'));
}

// Handshake from PACKAGES_DATA["init"] (trailing zeros are padded automatically).
inline std::vector<QByteArray> initSequence() {
    static constexpr const char* kPrefixes[] = {
        "071f", "071f", "0705", "071f", "071f", "071f", "0729", "071f", "071f", "071f",
        "0705", "0707", "071f", "071f", "072a", "071f", "071f", "072a", "071f", "071f",
        "0729", "071e01", "07090000000000000100", "0705", "072f002e",
        "070c0000000000000680", "070c0000000000000680",
        "07030605", "0706", "07030601", "07030601", "07030605", "07030601", "07030601"};
    std::vector<QByteArray> out;
    for (const char* p : kPrefixes) out.push_back(packetFromHexPrefix(p));
    return out;
}

inline QByteArray finalPacket() { return packetFromHexPrefix("0705"); }

// One byte per LED per channel. Order matches the original script: B, G, R.
inline std::vector<QByteArray> colorPackets(const std::vector<QRgb> &leds) {
    enum class ColorChannelId : char { Blue = 0x0b, Green = 0x09, Red = 0x07 };

    struct ColorChannel {
        int shift;
        ColorChannelId colorId;
    };

    static constexpr std::array<ColorChannel, 3> channels{
        {{0, ColorChannelId::Blue}, {8, ColorChannelId::Green}, {16, ColorChannelId::Red}}};

    //
    std::vector<QByteArray> out;

    QByteArray p;
    p.reserve(kPacketBytes);
    p.resize(6);

    // 0 [color id],
    p[1] = 0x03;
    p[2] = 0x06;
    // 3 [color id + half]
    // 4 0x00
    // 5 0x00

    std::ranges::for_each(channels, [&](const ColorChannel &ch) {
        for (char half = 0, halfId = 0; half < 2; ++half) {
            halfId += half;

            // Clear old data
            auto it = std::begin(p);
            std::advance(it, 6);
            p.erase(it, p.end());

            p[0] = static_cast<char>(ch.colorId);
            p[3] = static_cast<char>(ch.colorId)+ halfId;

            for (int j = 0; j < kLedsPerPacket; ++j) {
                const std::size_t idx = std::size_t(half * kLedsPerPacket + j);
                const QRgb c = idx < leds.size() ? leds[idx] : qRgb(0, 0, 0);
                p.push_back(char((c >> ch.shift) & 0xff));
            }
            out.push_back(p);
        }
    });

    return out;
}

}  // namespace bloody
