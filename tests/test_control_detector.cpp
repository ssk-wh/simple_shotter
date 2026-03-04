#include <QtTest>
#include "core/control_detector.h"
#include "mock_platform_api.h"

using namespace easyshotter;

class TestControlDetector : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        m_mock = std::make_unique<MockPlatformApi>();
        m_detector = std::make_unique<ControlDetector>(m_mock.get());
    }

    void cleanup()
    {
        m_detector.reset();
        m_mock.reset();
    }

    // --- findControlAt ---

    void test_findControlAt_returnsControlInfo()
    {
        ControlInfo expected;
        expected.type = ControlType::Button;
        expected.rect = QRect(10, 10, 80, 30);
        expected.name = "OK";
        expected.className = "QPushButton";
        m_mock->mockControlAtPoint = expected;

        ControlInfo result = m_detector->findControlAt(QPoint(50, 25));
        QCOMPARE(result.type, ControlType::Button);
        QCOMPARE(result.rect, QRect(10, 10, 80, 30));
        QCOMPARE(result.name, QString("OK"));
    }

    void test_findControlAt_returnsUnknownWhenNoControl()
    {
        ControlInfo empty;
        m_mock->mockControlAtPoint = empty;

        ControlInfo result = m_detector->findControlAt(QPoint(999, 999));
        QCOMPARE(result.type, ControlType::Unknown);
    }

    // --- Throttling mechanism ---

    void test_throttle_samePosition_returnsCached()
    {
        ControlInfo ctrl;
        ctrl.type = ControlType::Edit;
        ctrl.rect = QRect(0, 0, 100, 30);
        m_mock->mockControlAtPoint = ctrl;

        // First call: actually queries the API
        m_detector->findControlAt(QPoint(50, 15));
        int callsAfterFirst = m_mock->controlAtPointCallCount;

        // Same position, valid cached result: should not call API again
        m_detector->findControlAt(QPoint(50, 15));
        QCOMPARE(m_mock->controlAtPointCallCount, callsAfterFirst);
    }

    void test_throttle_rapidCalls_returnsCached()
    {
        ControlInfo ctrl;
        ctrl.type = ControlType::Label;
        ctrl.rect = QRect(0, 0, 50, 20);
        m_mock->mockControlAtPoint = ctrl;

        // First call triggers real query
        m_detector->findControlAt(QPoint(10, 10));
        int callsAfterFirst = m_mock->controlAtPointCallCount;

        // Rapid call with different position but within throttle window
        // Should return cached result
        ControlInfo result = m_detector->findControlAt(QPoint(20, 20));
        QCOMPARE(result.type, ControlType::Label);
        QCOMPARE(m_mock->controlAtPointCallCount, callsAfterFirst);
    }

    void test_throttle_afterDelay_queriesAgain()
    {
        ControlInfo ctrl1;
        ctrl1.type = ControlType::Button;
        ctrl1.rect = QRect(0, 0, 80, 30);
        m_mock->mockControlAtPoint = ctrl1;

        m_detector->findControlAt(QPoint(40, 15));
        int callsAfterFirst = m_mock->controlAtPointCallCount;

        // Wait beyond throttle window (80ms)
        QTest::qWait(100);

        ControlInfo ctrl2;
        ctrl2.type = ControlType::Edit;
        ctrl2.rect = QRect(100, 0, 200, 30);
        m_mock->mockControlAtPoint = ctrl2;

        ControlInfo result = m_detector->findControlAt(QPoint(200, 15));
        QVERIFY(m_mock->controlAtPointCallCount > callsAfterFirst);
        QCOMPARE(result.type, ControlType::Edit);
    }

    // --- Position change ---

    void test_positionChange_afterThrottle_queriesNewPosition()
    {
        ControlInfo ctrl;
        ctrl.type = ControlType::CheckBox;
        ctrl.rect = QRect(0, 0, 20, 20);
        m_mock->mockControlAtPoint = ctrl;

        m_detector->findControlAt(QPoint(10, 10));

        // Wait for throttle to expire
        QTest::qWait(100);

        ControlInfo newCtrl;
        newCtrl.type = ControlType::ComboBox;
        newCtrl.rect = QRect(200, 200, 100, 30);
        m_mock->mockControlAtPoint = newCtrl;

        ControlInfo result = m_detector->findControlAt(QPoint(250, 215));
        QCOMPARE(result.type, ControlType::ComboBox);
    }

private:
    std::unique_ptr<MockPlatformApi> m_mock;
    std::unique_ptr<ControlDetector> m_detector;
};

QTEST_MAIN(TestControlDetector)
#include "test_control_detector.moc"
