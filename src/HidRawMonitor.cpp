#include "HidRawMonitor.hpp"

#include <stdexcept>

void HidrawMonitor::setup(const uint16_t target_vid, const uint16_t target_pid) {

    target_vid_ = target_vid;
    target_pid_ = target_pid;

    if (udev_.reset(udev_new()); !udev_) {
        throw std::runtime_error("Failed to initialize udev context");
    }

    if (mon_.reset(udev_monitor_new_from_netlink(udev_.get(), "udev")); !mon_) {        
        udev_.reset(nullptr);
        mon_.reset(nullptr);
        throw std::runtime_error("Failed to create udev monitor");
    }

    udev_monitor_filter_add_match_subsystem_devtype(mon_.get(), "hidraw", nullptr);
    udev_monitor_enable_receiving(mon_.get());
    fd_ = udev_monitor_get_fd(mon_.get());
}

HidrawMonitor::~HidrawMonitor() {
    // stop();
    if (mon_){        
        mon_.reset(nullptr);
    }
    if (udev_){        
        udev_.reset(nullptr);
    }
}

void HidrawMonitor::onConnect(DeviceCallback cb) {
    on_connect_ = std::move(cb);
}

void HidrawMonitor::onDisconnect(DeviceCallback cb) {
    on_disconnect_ = std::move(cb);
}

void HidrawMonitor::run(std::stop_token &token) {
    struct pollfd fds{fd_, POLLIN, 0};

    while (!token.stop_requested()) {
        // 250ms timeout allows checking running_ flag cleanly without signals
        int ret = poll(&fds, 1, 250);
        if (ret > 0 && (fds.revents & POLLIN)) {
            processNextEvent();
        }
    }
}

void HidrawMonitor::processNextEvent() {
    struct udev_device *dev = udev_monitor_receive_device(mon_.get());
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

                if (!vid || !pid)
                    return;
                auto tVid = std::stoi(vid, nullptr, 16);
                auto tPid = std::stoi(pid, nullptr, 16);
                if (target_vid_ == tVid && target_pid_ == tPid) {
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
