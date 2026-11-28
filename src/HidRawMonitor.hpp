#pragma once

#include <stop_token>
#pragma once

#include <functional>
#include <libudev.h>
#include <poll.h>
#include <string>
#include <unistd.h>
#include <memory>

class HidrawMonitor {
    struct UdevDeleter {
        void operator()(struct udev* u) const noexcept { udev_unref(u); }
        void operator()(struct udev_device* d) const noexcept { udev_device_unref(d); }
        void operator()(struct udev_enumerate* e) const noexcept { udev_enumerate_unref(e); }
        void operator()(struct udev_monitor* m) const noexcept { udev_monitor_unref(m); }
        void operator()(struct udev_list_entry* ) const noexcept { /* owned by parent, no unref needed */ }
    };

  public:
    using DeviceCallback = std::function<void(const std::string &devnode)>;

    HidrawMonitor() = default;

    // Destructor automatically cleans up C resources (RAII)
    ~HidrawMonitor();

    // Prevent copying to avoid double-freeing udev pointers
    HidrawMonitor(const HidrawMonitor &) = delete;
    HidrawMonitor &operator=(const HidrawMonitor &) = delete;

    // Register callbacks for connection events
    void onConnect(DeviceCallback cb);
    void onDisconnect(DeviceCallback cb);

    // Expose the file descriptor if you want to integrate with an external poll/epoll loop
    int getFd() const { return fd_; }

    // Blocking event loop (can be run on a dedicated std::thread)
    void run(std::stop_token& token);

    // Processes a single udev event (call this directly if using an external poll/epoll)
    void processNextEvent();

    // Prepares udev for monitoring
    void setup(const uint16_t target_vid, const uint16_t target_pid);

  private:
    uint16_t target_vid_;
    uint16_t target_pid_;
    std::string active_devnode_;

    std::unique_ptr<struct udev, UdevDeleter> udev_{nullptr};
    std::unique_ptr<struct udev_monitor, UdevDeleter> mon_{nullptr};
    int fd_ = -1;

    DeviceCallback on_connect_;
    DeviceCallback on_disconnect_;    
};
