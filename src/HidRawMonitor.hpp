#pragma once

#pragma once

#include <atomic>
#include <functional>
#include <libudev.h>
#include <poll.h>
#include <string>
#include <unistd.h>

class HidrawMonitor {
  public:
    using DeviceCallback = std::function<void(const std::string &devnode)>;

    HidrawMonitor(std::string target_vid, std::string target_pid);

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
    void run();

    void stop() { running_ = false; }

    // Processes a single udev event (call this directly if using an external poll/epoll)
    void processNextEvent();

  private:
    std::string target_vid_;
    std::string target_pid_;
    std::string active_devnode_;

    struct udev *udev_ = nullptr;
    struct udev_monitor *mon_ = nullptr;
    int fd_ = -1;

    std::atomic<bool> running_;
    DeviceCallback on_connect_;
    DeviceCallback on_disconnect_;
};
