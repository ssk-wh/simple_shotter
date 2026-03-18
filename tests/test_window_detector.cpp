#include <QtTest>
#include "core/window_detector.h"
#include "mock_platform_api.h"

using namespace simpleshotter;

class TestWindowDetector : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        m_mock = std::make_unique<MockPlatformApi>();
        m_detector = std::make_unique<WindowDetector>(m_mock.get());
    }

    void cleanup()
    {
        m_detector.reset();
        m_mock.reset();
    }

    // --- refresh / windows ---

    void test_refresh_populatesWindowList()
    {
        WindowInfo w1;
        w1.handle = 1;
        w1.title = "Window 1";
        w1.rect = QRect(0, 0, 800, 600);
        w1.zOrder = 0;

        WindowInfo w2;
        w2.handle = 2;
        w2.title = "Window 2";
        w2.rect = QRect(100, 100, 400, 300);
        w2.zOrder = 1;

        m_mock->mockWindows = {w1, w2};
        m_detector->refresh();

        QCOMPARE(static_cast<int>(m_detector->windows().size()), 2);
        QCOMPARE(m_detector->windows()[0].title, QString("Window 1"));
        QCOMPARE(m_detector->windows()[1].title, QString("Window 2"));
    }

    void test_refresh_emptyList()
    {
        m_mock->mockWindows.clear();
        m_detector->refresh();
        QVERIFY(m_detector->windows().empty());
    }

    void test_refresh_replacesOldData()
    {
        WindowInfo w;
        w.handle = 1;
        w.rect = QRect(0, 0, 100, 100);
        m_mock->mockWindows = {w};
        m_detector->refresh();
        QCOMPARE(static_cast<int>(m_detector->windows().size()), 1);

        m_mock->mockWindows.clear();
        m_detector->refresh();
        QVERIFY(m_detector->windows().empty());
    }

    // --- findWindowAt ---

    void test_findWindowAt_matchesByZOrder()
    {
        // Windows listed in Z-order: front window first
        WindowInfo front;
        front.handle = 10;
        front.title = "Front";
        front.rect = QRect(50, 50, 200, 200);
        front.zOrder = 0;

        WindowInfo back;
        back.handle = 20;
        back.title = "Back";
        back.rect = QRect(0, 0, 500, 500);
        back.zOrder = 1;

        m_mock->mockWindows = {front, back};
        m_detector->refresh();

        // Point inside both windows - should match the first (front)
        WindowInfo result = m_detector->findWindowAt(QPoint(100, 100));
        QCOMPARE(result.handle, quintptr(10));
        QCOMPARE(result.title, QString("Front"));
    }

    void test_findWindowAt_returnsEmpty_whenNoMatch()
    {
        WindowInfo w;
        w.handle = 1;
        w.rect = QRect(100, 100, 200, 200);
        m_mock->mockWindows = {w};
        m_detector->refresh();

        WindowInfo result = m_detector->findWindowAt(QPoint(0, 0));
        QCOMPARE(result.handle, quintptr(0));
    }

    void test_findWindowAt_matchesSingleWindow()
    {
        WindowInfo w;
        w.handle = 5;
        w.title = "Only";
        w.rect = QRect(10, 10, 300, 300);
        m_mock->mockWindows = {w};
        m_detector->refresh();

        WindowInfo result = m_detector->findWindowAt(QPoint(150, 150));
        QCOMPARE(result.handle, quintptr(5));
    }

    void test_findWindowAt_pointOnEdge()
    {
        WindowInfo w;
        w.handle = 7;
        w.rect = QRect(100, 100, 200, 200);
        m_mock->mockWindows = {w};
        m_detector->refresh();

        // Top-left corner of rect
        WindowInfo result = m_detector->findWindowAt(QPoint(100, 100));
        QCOMPARE(result.handle, quintptr(7));
    }

    // --- Multi-window overlap scenarios ---

    void test_findWindowAt_threeOverlappingWindows()
    {
        WindowInfo w1;
        w1.handle = 1;
        w1.title = "Top";
        w1.rect = QRect(100, 100, 100, 100);

        WindowInfo w2;
        w2.handle = 2;
        w2.title = "Middle";
        w2.rect = QRect(50, 50, 200, 200);

        WindowInfo w3;
        w3.handle = 3;
        w3.title = "Bottom";
        w3.rect = QRect(0, 0, 400, 400);

        m_mock->mockWindows = {w1, w2, w3};
        m_detector->refresh();

        // Point in all three - first match wins
        WindowInfo r = m_detector->findWindowAt(QPoint(150, 150));
        QCOMPARE(r.handle, quintptr(1));

        // Point only in w2 and w3
        WindowInfo r2 = m_detector->findWindowAt(QPoint(60, 60));
        QCOMPARE(r2.handle, quintptr(2));

        // Point only in w3
        WindowInfo r3 = m_detector->findWindowAt(QPoint(10, 10));
        QCOMPARE(r3.handle, quintptr(3));
    }

    void test_findWindowAt_nonOverlappingWindows()
    {
        WindowInfo w1;
        w1.handle = 1;
        w1.rect = QRect(0, 0, 100, 100);

        WindowInfo w2;
        w2.handle = 2;
        w2.rect = QRect(200, 200, 100, 100);

        m_mock->mockWindows = {w1, w2};
        m_detector->refresh();

        QCOMPARE(m_detector->findWindowAt(QPoint(50, 50)).handle, quintptr(1));
        QCOMPARE(m_detector->findWindowAt(QPoint(250, 250)).handle, quintptr(2));
        QCOMPARE(m_detector->findWindowAt(QPoint(150, 150)).handle, quintptr(0));
    }

private:
    std::unique_ptr<MockPlatformApi> m_mock;
    std::unique_ptr<WindowDetector> m_detector;
};

QTEST_MAIN(TestWindowDetector)
#include "test_window_detector.moc"
