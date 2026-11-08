#include "ScreenWatcher.hpp"
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>

namespace {
constexpr const char* kMutterService = "org.gnome.Mutter.DisplayConfig";
constexpr const char* kMutterPath = "/org/gnome/Mutter/DisplayConfig";
}

ScreenWatcher::ScreenWatcher(QObject* parent) : QObject(parent) {
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (bus.isConnected()) {
        mutter_ = new QDBusInterface(kMutterService, kMutterPath, kMutterService, bus, this);
        const QVariant mode = mutter_->isValid() ? mutter_->property("PowerSaveMode") : QVariant();
        if (mode.isValid()) {
            hasMonitorSource_ = true;
            monitorOff_ = mode.toInt() > 0;     // 0 = on, 1..3 = standby/suspend/off
            bus.connect(kMutterService, kMutterPath, "org.freedesktop.DBus.Properties", "PropertiesChanged",
                        this, SLOT(onMutterProperties(QString, QVariantMap, QStringList)));
            sources_ << "GNOME display power state";
        }

        struct Lock { const char* service; const char* path; const char* iface; };
        const Lock locks[] = {
            {"org.gnome.ScreenSaver", "/org/gnome/ScreenSaver", "org.gnome.ScreenSaver"},
            {"org.freedesktop.ScreenSaver", "/org/freedesktop/ScreenSaver", "org.freedesktop.ScreenSaver"},
            {"org.freedesktop.ScreenSaver", "/ScreenSaver", "org.freedesktop.ScreenSaver"},
        };
        for (const Lock& l : locks) {
            bus.connect(QString(), l.path, l.iface, "ActiveChanged", this, SLOT(onLockChanged(bool)));
            QDBusInterface i(l.service, l.path, l.iface, bus);
            if (i.isValid()) {
                const QDBusReply<bool> r = i.call("GetActive");
                if (r.isValid() && r.value()) locked_ = true;
            }
        }
        sources_ << "screen lock";
    }

    if (!hasMonitorSource_ && qEnvironmentVariable("XDG_SESSION_TYPE") == "x11") {
        xset_ = new QProcess(this);
        connect(xset_, &QProcess::errorOccurred, this, [this] { pollTimer_.stop(); });
        connect(xset_, &QProcess::finished, this, [this] {
            const QString out = QString::fromUtf8(xset_->readAllStandardOutput());
            if (out.contains("DPMS is Enabled"))
                setMonitorOff(out.contains("Monitor is Off") || out.contains("Monitor is in Standby") ||
                              out.contains("Monitor is in Suspend"));
        });
        connect(&pollTimer_, &QTimer::timeout, this, &ScreenWatcher::pollDpms);
        pollTimer_.start(2000);
        hasMonitorSource_ = true;
        sources_ << "X11 DPMS (xset)";
    }
}

void ScreenWatcher::onMutterProperties(const QString& iface, const QVariantMap& props, const QStringList& invalidated) {
    if (iface != kMutterService) return;
    if (props.contains("PowerSaveMode")) setMonitorOff(props.value("PowerSaveMode").toInt() > 0);
    else if (invalidated.contains("PowerSaveMode") && mutter_) setMonitorOff(mutter_->property("PowerSaveMode").toInt() > 0);
}

void ScreenWatcher::onLockChanged(bool active) { setLocked(active); }

void ScreenWatcher::pollDpms() {
    if (xset_->state() == QProcess::NotRunning) xset_->start("xset", {"q"});
}

void ScreenWatcher::setMonitorOff(bool v) {
    if (v == monitorOff_) return;
    monitorOff_ = v;
    emit changed();
}

void ScreenWatcher::setLocked(bool v) {
    if (v == locked_) return;
    locked_ = v;
    emit changed();
}
