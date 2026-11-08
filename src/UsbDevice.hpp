#pragma once
#include <QByteArray>
#include <QString>
#include <cstdint>

struct libusb_context;
struct libusb_device_handle;

// Two backends:
//  * hidraw (default): HIDIOCSFEATURE on the interface-2 hidraw node. Same
//    bytes as the original control transfer, but the kernel driver stays
//    bound, so the keyboard keeps typing.
//  * libusb (legacy): detaches the kernel driver; keys stop working while open.
class UsbDevice {
public:
    UsbDevice() = default;
    ~UsbDevice() { close(); }
    UsbDevice(const UsbDevice&) = delete;
    UsbDevice& operator=(const UsbDevice&) = delete;

    bool open(std::uint16_t vid, std::uint16_t pid, bool preferHidraw, bool resetFirst, QString* error);
    void close();
    bool isOpen() const { return fd_ >= 0 || handle_ != nullptr; }
    bool send(const QByteArray& packet, QString* error);
    QString backend() const { return backend_; }

private:
    bool openHidraw(std::uint16_t vid, std::uint16_t pid, QString* error);
    bool openLibusb(std::uint16_t vid, std::uint16_t pid, QString* error);
    static bool resetConfiguration(std::uint16_t vid, std::uint16_t pid, QString* error);

    int fd_ = -1;
    libusb_context* ctx_ = nullptr;
    libusb_device_handle* handle_ = nullptr;
    QString backend_;
};
