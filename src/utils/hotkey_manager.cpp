#include "hotkey_manager.h"

namespace simpleshotter {

HotkeyManager::HotkeyManager(PlatformApi* api, QObject* parent)
    : QObject(parent)
    , m_api(api)
{
}

HotkeyManager::~HotkeyManager()
{
    unregisterAll();
}

bool HotkeyManager::registerHotkey(Qt::Key key, Qt::KeyboardModifiers modifiers)
{
    int id = m_api->registerHotkey(key, modifiers, [this]() {
        emit hotkeyTriggered();
    });
    if (id < 0) return false;
    m_registeredIds.append(id);
    return true;
}

void HotkeyManager::unregisterAll()
{
    for (int id : m_registeredIds) {
        m_api->unregisterHotkey(id);
    }
    m_registeredIds.clear();
}

} // namespace simpleshotter
