#pragma once

#include <QObject>
#include "platform/platform_api.h"

namespace simpleshotter {

class HotkeyManager : public QObject {
    Q_OBJECT
public:
    explicit HotkeyManager(PlatformApi* api, QObject* parent = nullptr);
    ~HotkeyManager();

    bool registerHotkey(Qt::Key key, Qt::KeyboardModifiers modifiers);
    void unregisterAll();

signals:
    void hotkeyTriggered();

private:
    PlatformApi* m_api;
    QList<int> m_registeredIds;
};

} // namespace simpleshotter
