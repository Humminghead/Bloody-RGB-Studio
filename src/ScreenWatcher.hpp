#pragma once
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>

class QDBusInterface;

// Detects "monitor is off" (and, separately, "screen is locked").
//  * GNOME/Mutter: org.gnome.Mutter.DisplayConfig PowerSaveMode (Wayland and X11)
//  * lock state:   ActiveChanged on org.gnome.ScreenSaver / org.freedesktop.ScreenSaver
//  * other X11 desktops: polls `xset q` for DPMS state
class ScreenWatcher : public QObject {
    Q_OBJECT
public:
    explicit ScreenWatcher(QObject* parent = nullptr);
    bool monitorOff() const { return monitorOff_; }
    bool locked() const { return locked_; }
    QString description() const { return sources_.join(", "); }
    bool hasMonitorSource() const { return hasMonitorSource_; }

signals:
    void changed();

private slots:
    void onMutterProperties(const QString& iface, const QVariantMap& changedProps, const QStringList& invalidated);
    void onLockChanged(bool active);
    void pollDpms();

private:
    void setMonitorOff(bool v);
    void setLocked(bool v);

    QDBusInterface* mutter_ = nullptr;
    QProcess* xset_ = nullptr;
    QTimer pollTimer_;
    QStringList sources_;
    bool monitorOff_ = false, locked_ = false, hasMonitorSource_ = false;
};
