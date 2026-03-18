#include <QtTest>
#include "utils/hotkey_manager.h"
#include "mock_platform_api.h"

using namespace simpleshotter;

class TestHotkeyManager : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        m_mock = std::make_unique<MockPlatformApi>();
        m_manager = std::make_unique<HotkeyManager>(m_mock.get());
    }

    void cleanup()
    {
        m_manager.reset();
        m_mock.reset();
    }

    // --- registerHotkey ---

    void test_registerHotkey_success()
    {
        m_mock->mockHotkeyId = 1;
        bool ok = m_manager->registerHotkey(Qt::Key_F1, Qt::NoModifier);
        QVERIFY(ok);
        QCOMPARE(m_mock->registerHotkeyCallCount, 1);
    }

    void test_registerHotkey_failure()
    {
        m_mock->mockHotkeyId = -1;  // Simulate failure
        bool ok = m_manager->registerHotkey(Qt::Key_F1, Qt::NoModifier);
        QVERIFY(!ok);
    }

    void test_registerHotkey_withModifiers()
    {
        m_mock->mockHotkeyId = 2;
        bool ok = m_manager->registerHotkey(Qt::Key_S, Qt::ControlModifier | Qt::ShiftModifier);
        QVERIFY(ok);
        QCOMPARE(m_mock->registerHotkeyCallCount, 1);
    }

    void test_registerMultipleHotkeys()
    {
        m_mock->mockHotkeyId = 1;
        m_manager->registerHotkey(Qt::Key_F1, Qt::NoModifier);
        m_manager->registerHotkey(Qt::Key_F2, Qt::NoModifier);
        m_manager->registerHotkey(Qt::Key_Print, Qt::NoModifier);

        QCOMPARE(m_mock->registerHotkeyCallCount, 3);
    }

    // --- unregisterAll ---

    void test_unregisterAll_unregistersAllHotkeys()
    {
        m_mock->mockHotkeyId = 1;
        m_manager->registerHotkey(Qt::Key_F1, Qt::NoModifier);
        m_mock->mockHotkeyId = 2;
        m_manager->registerHotkey(Qt::Key_F2, Qt::NoModifier);

        m_manager->unregisterAll();
        QCOMPARE(m_mock->unregisterHotkeyCallCount, 2);
    }

    void test_unregisterAll_withNoRegistered()
    {
        m_manager->unregisterAll();
        QCOMPARE(m_mock->unregisterHotkeyCallCount, 0);
    }

    void test_unregisterAll_clearsInternalList()
    {
        m_mock->mockHotkeyId = 1;
        m_manager->registerHotkey(Qt::Key_F1, Qt::NoModifier);
        m_manager->unregisterAll();
        QCOMPARE(m_mock->unregisterHotkeyCallCount, 1);

        // Second unregisterAll should not call unregister again
        m_mock->unregisterHotkeyCallCount = 0;
        m_manager->unregisterAll();
        QCOMPARE(m_mock->unregisterHotkeyCallCount, 0);
    }

    // --- Callback triggers signal ---

    void test_hotkeyCallback_triggersSignal()
    {
        m_mock->mockHotkeyId = 1;
        m_manager->registerHotkey(Qt::Key_F1, Qt::NoModifier);

        QSignalSpy spy(m_manager.get(), &HotkeyManager::hotkeyTriggered);
        QVERIFY(m_mock->lastHotkeyCallback);

        // Simulate hotkey press via callback
        m_mock->lastHotkeyCallback();
        QCOMPARE(spy.count(), 1);
    }

    // --- Destructor cleanup ---

    void test_destructor_unregistersAll()
    {
        auto* rawMock = m_mock.get();
        auto manager = std::make_unique<HotkeyManager>(rawMock);

        rawMock->mockHotkeyId = 1;
        manager->registerHotkey(Qt::Key_F1, Qt::NoModifier);
        rawMock->mockHotkeyId = 2;
        manager->registerHotkey(Qt::Key_F2, Qt::NoModifier);

        rawMock->unregisterHotkeyCallCount = 0;
        manager.reset();  // Destructor called

        QCOMPARE(rawMock->unregisterHotkeyCallCount, 2);
    }

private:
    std::unique_ptr<MockPlatformApi> m_mock;
    std::unique_ptr<HotkeyManager> m_manager;
};

QTEST_MAIN(TestHotkeyManager)
#include "test_hotkey_manager.moc"
