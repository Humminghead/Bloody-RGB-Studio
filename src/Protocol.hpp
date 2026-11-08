#pragma once
// Port of rgb_utils.py. Packets are 64 bytes, sent as HID class
// SET_REPORT (bmRequestType 0x21, bRequest 0x09, wValue 0x0307, wIndex 2).
#include <QByteArray>
#include <QRgb>
#include <vector>

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
inline std::vector<QByteArray> colorPackets(const std::vector<QRgb>& leds) {
    struct Channel { int shift; int firstId; };
    static constexpr Channel kChannels[] = {{0, 0x0b}, {8, 0x09}, {16, 0x07}};
    std::vector<QByteArray> out;
    for (const auto& ch : kChannels) {
        for (int half = 0; half < 2; ++half) {
            QByteArray p;
            for (int b : {ch.firstId, 0x03, 0x06, ch.firstId + half, 0x00, 0x00}) {
                p.append(char(b));
            }
            for (int j = 0; j < kLedsPerPacket; ++j) {
                const std::size_t idx = std::size_t(half * kLedsPerPacket + j);
                const QRgb c = idx < leds.size() ? leds[idx] : qRgb(0, 0, 0);
                p.append(char((c >> ch.shift) & 0xff));
            }
            out.push_back(p);
        }
    }
    return out;
}

}  // namespace bloody
