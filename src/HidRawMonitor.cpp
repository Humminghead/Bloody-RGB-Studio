#include "HidRawMonitor.hpp"

#include <stdexcept>

HidrawMonitor::HidrawMonitor(std::string target_vid, std::string target_pid)
    : target_vid_(std::move(target_vid)), target_pid_(std::move(target_pid)), running_(false) {

    udev_ = udev_new();
    if (!udev_) {
        throw std::runtime_error("Failed to initialize udev context");
    }

    mon_ = udev_monitor_new_from_netlink(udev_, "udev");
    if (!mon_) {
        udev_unref(udev_);
        throw std::runtime_error("Failed to create udev monitor");
    }

    udev_monitor_filter_add_match_subsystem_devtype(mon_, "hidraw", nullptr);
    udev_monitor_enable_receiving(mon_);
    fd_ = udev_monitor_get_fd(mon_);
}

HidrawMonitor::~HidrawMonitor() {
    stop();
    if (mon_)
        udev_monitor_unref(mon_);
    if (udev_)
        udev_unref(udev_);
}

void HidrawMonitor::onConnect(DeviceCallback cb) {
    on_connect_ = std::move(cb);
}

void HidrawMonitor::onDisconnect(DeviceCallback cb) {
    on_disconnect_ = std::move(cb);
}

void HidrawMonitor::run() {
    running_ = true;
    struct pollfd fds{fd_, POLLIN, 0};

    while (running_) {
        // 250ms timeout allows checking running_ flag cleanly without signals
        int ret = poll(&fds, 1, 250);
        if (ret > 0 && (fds.revents & POLLIN)) {
            processNextEvent();
        }
    }
}

void HidrawMonitor::processNextEvent() {
    struct udev_device *dev = udev_monitor_receive_device(mon_);
    if (!dev)
        return;

    const char *action_c = udev_device_get_action(dev);
    const char *devnode_c = udev_device_get_devnode(dev);

    if (action_c && devnode_c) {
        std::string action(action_c);
        std::string devnode(devnode_c);

        if (action == "add") {
            struct udev_device *usb_dev = udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_device");

            if (usb_dev) {
                const char *vid = udev_device_get_sysattr_value(usb_dev, "idVendor");
                const char *pid = udev_device_get_sysattr_value(usb_dev, "idProduct");

                if (vid && pid && target_vid_ == vid && target_pid_ == pid) {
                    active_devnode_ = devnode; // Track node for reliable disconnect matching
                    if (on_connect_)
                        on_connect_(devnode);
                }
            }
        } else if (action == "remove") {
            // Match against the tracked devnode since sysfs attributes may already be gone
            if (!active_devnode_.empty() && active_devnode_ == devnode) {
                active_devnode_.clear();
                if (on_disconnect_)
                    on_disconnect_(devnode);
            }
        }
    }

    udev_device_unref(dev);
}
