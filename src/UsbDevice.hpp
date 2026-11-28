#pragma once
#include "HidRawMonitor.hpp"
#include <QByteArray>
#include <QString>
#include <cstdint>
#include <memory>
#include <thread>

struct libusb_context;
struct libusb_device_handle;

// Two backends:
//  * hidraw (default): HIDIOCSFEATURE on the interface-2 hidraw node. Same
//    bytes as the original control transfer, but the kernel driver stays
//    bound, so the keyboard keeps typing.
//  * libusb (legacy): detaches the kernel driver; keys stop working while open.

class UsbDevice {
public:
    enum class Status{
        UsbConnected,
        UsbDisconnected
    };

    UsbDevice() = default;
    ~UsbDevice() { close(); }
    UsbDevice(const UsbDevice&) = delete;
    UsbDevice& operator=(const UsbDevice&) = delete;

    bool open(std::uint16_t vid, std::uint16_t pid, bool preferHidraw, bool resetFirst, QString* error);    
    void close();
    bool isOpen() const;
    bool send(const QByteArray& packet, QString* error);
    QString backend() const { return backend_; }

    bool openHidRaw(std::uint16_t vid, std::uint16_t pid, QString* error = nullptr, QString* node = nullptr);
    bool openLibUsb(std::uint16_t vid, std::uint16_t pid, QString* error);
private:
    static auto createHidId(std::uint16_t vid, std::uint16_t pid) {
        return QString("HID_ID=0003:%1:%2").arg(vid, 8, 16, QChar('0')).arg(pid, 8, 16, QChar('0')).toUpper();
    }
    std::optional<QString> openHidNode(const QString &name, const QString& hidId);
    static bool resetConfiguration(std::uint16_t vid, std::uint16_t pid, QString* error);

    int fd_ = -1;
    libusb_context* ctx_ = nullptr;
    libusb_device_handle* handle_ = nullptr;
    QString backend_;
    std::unique_ptr<HidrawMonitor> hidmon_{nullptr};
    std::jthread hidmon_th_;
    Status status_{Status::UsbDisconnected};
};
