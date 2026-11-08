#include "UsbDevice.hpp"
#include <QDir>
#include <QFile>
#include <QThread>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <libusb-1.0/libusb.h>
#include <linux/hidraw.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace {
constexpr int kInterface = 2;
constexpr std::uint8_t kRequestType = 0x21;
constexpr std::uint8_t kRequest = 0x09;
constexpr std::uint16_t kValue = 0x0307;   // feature report (0x03), report ID 7
constexpr unsigned kTimeoutMs = 1000;
}

bool UsbDevice::open(std::uint16_t vid, std::uint16_t pid, bool preferHidraw, bool resetFirst, QString* error) {
    close();
    if (!preferHidraw) return openLibusb(vid, pid, error);
    if (!resetFirst) return openHidraw(vid, pid, error);

    // Same effect as pyusb's set_configuration() in the original tool: the kernel
    // re-creates all interfaces (the keyboard re-binds within a moment), then we
    // wait for the hidraw node to come back.
    QString ignored;
    resetConfiguration(vid, pid, &ignored);
    for (int i = 0; i < 40; ++i) {
        if (openHidraw(vid, pid, error)) return true;
        QThread::msleep(100);
    }
    return false;
}

bool UsbDevice::resetConfiguration(std::uint16_t vid, std::uint16_t pid, QString* error) {
    libusb_context* ctx = nullptr;
    if (libusb_init(&ctx) < 0) return false;
    bool ok = false;
    if (libusb_device_handle* h = libusb_open_device_with_vid_pid(ctx, vid, pid)) {
        libusb_config_descriptor* cfg = nullptr;
        if (libusb_get_active_config_descriptor(libusb_get_device(h), &cfg) == 0) {
            const int value = cfg->bConfigurationValue;
            libusb_free_config_descriptor(cfg);
            const int rc = libusb_set_configuration(h, value);
            ok = rc == 0;
            if (!ok && error) *error = libusb_strerror(rc);
        }
        libusb_close(h);
    } else if (error) {
        *error = "device not found for reset";
    }
    libusb_exit(ctx);
    return ok;
}

bool UsbDevice::openHidraw(std::uint16_t vid, std::uint16_t pid, QString* error) {
    const QString want = QString("HID_ID=0003:%1:%2").arg(vid, 8, 16, QChar('0')).arg(pid, 8, 16, QChar('0')).toUpper();
    QString lastError = "No hidraw node found for interface 2 of this keyboard.";

    const QStringList nodes = QDir("/sys/class/hidraw").entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
    for (const QString& name : nodes) {
        const QString base = "/sys/class/hidraw/" + name + "/device/";
        QFile uevent(base + "uevent");
        if (!uevent.open(QIODevice::ReadOnly) || !QString::fromUtf8(uevent.readAll()).contains(want)) continue;

        QFile ifc(base + "../bInterfaceNumber");
        if (!ifc.open(QIODevice::ReadOnly) || QString::fromUtf8(ifc.readAll()).trimmed().toInt(nullptr, 16) != kInterface)
            continue;

        fd_ = ::open(("/dev/" + name).toLocal8Bit().constData(), O_RDWR);
        if (fd_ >= 0) { backend_ = "hidraw (/dev/" + name + ")"; return true; }
        lastError = "Cannot open /dev/" + name + ": " + std::strerror(errno) + " (install the udev rule, re-plug)";
    }
    if (error) *error = lastError;
    return false;
}

bool UsbDevice::openLibusb(std::uint16_t vid, std::uint16_t pid, QString* error) {
    auto fail = [&](const QString& msg) { if (error) *error = msg; close(); return false; };
    if (int rc = libusb_init(&ctx_); rc < 0) return fail(QString("libusb init failed: ") + libusb_strerror(rc));
    handle_ = libusb_open_device_with_vid_pid(ctx_, vid, pid);
    if (!handle_) return fail("Keyboard not found, or no permission (install the udev rule).");
    libusb_set_auto_detach_kernel_driver(handle_, 1);
    if (int rc = libusb_claim_interface(handle_, kInterface); rc < 0)
        return fail(QString("Cannot claim interface: ") + libusb_strerror(rc));
    backend_ = "libusb (legacy)";
    return true;
}

void UsbDevice::close() {
    if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
    if (handle_) { libusb_release_interface(handle_, kInterface); libusb_close(handle_); handle_ = nullptr; }
    if (ctx_) { libusb_exit(ctx_); ctx_ = nullptr; }
    backend_.clear();
}

bool UsbDevice::send(const QByteArray& packet, QString* error) {
    if (fd_ >= 0) {
        const int rc = ::ioctl(fd_, HIDIOCSFEATURE(packet.size()), const_cast<char*>(packet.constData()));
        if (rc < 0) { if (error) *error = std::strerror(errno); return false; }
        return true;
    }
    if (!handle_) return false;
    const int rc = libusb_control_transfer(handle_, kRequestType, kRequest, kValue, kInterface,
        reinterpret_cast<unsigned char*>(const_cast<char*>(packet.constData())),
        std::uint16_t(packet.size()), kTimeoutMs);
    if (rc < 0) { if (error) *error = QString::fromUtf8(libusb_strerror(rc)); return false; }
    return true;
}
