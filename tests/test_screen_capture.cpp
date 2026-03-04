#include <QtTest>
#include <QSignalSpy>
#include "core/screen_capture.h"
#include "mock_platform_api.h"

using namespace easyshotter;

class TestScreenCapture : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        m_mock = std::make_unique<MockPlatformApi>();
        m_capture = std::make_unique<ScreenCapture>(m_mock.get());
    }

    void cleanup()
    {
        m_capture.reset();
        m_mock.reset();
    }

    // --- captureFullScreen ---

    void test_captureFullScreen_returnsNonNull()
    {
        QPixmap result = m_capture->captureFullScreen();
        QVERIFY(!result.isNull());
    }

    void test_captureFullScreen_correctSize()
    {
        QPixmap result = m_capture->captureFullScreen();
        QCOMPARE(result.size(), QSize(1920, 1080));
    }

    void test_captureFullScreen_emitsSignal()
    {
        QSignalSpy spy(m_capture.get(), &ScreenCapture::captureCompleted);
        m_capture->captureFullScreen();
        QCOMPARE(spy.count(), 1);
    }

    void test_captureFullScreen_signalCarriesPixmap()
    {
        QSignalSpy spy(m_capture.get(), &ScreenCapture::captureCompleted);
        m_capture->captureFullScreen();

        QCOMPARE(spy.count(), 1);
        QPixmap emitted = spy.first().first().value<QPixmap>();
        QVERIFY(!emitted.isNull());
        QCOMPARE(emitted.size(), QSize(1920, 1080));
    }

    // --- captureRegion ---

    void test_captureRegion_returnsNonNull()
    {
        QPixmap result = m_capture->captureRegion(QRect(0, 0, 100, 100));
        QVERIFY(!result.isNull());
    }

    void test_captureRegion_emitsSignal()
    {
        QSignalSpy spy(m_capture.get(), &ScreenCapture::captureCompleted);
        m_capture->captureRegion(QRect(50, 50, 200, 150));
        QCOMPARE(spy.count(), 1);
    }

    void test_captureRegion_signalCarriesPixmap()
    {
        QSignalSpy spy(m_capture.get(), &ScreenCapture::captureCompleted);
        m_capture->captureRegion(QRect(0, 0, 100, 100));

        QPixmap emitted = spy.first().first().value<QPixmap>();
        QVERIFY(!emitted.isNull());
    }

    // --- Custom mock pixmap ---

    void test_captureFullScreen_withCustomPixmap()
    {
        QImage img(2560, 1440, QImage::Format_ARGB32);
        img.fill(Qt::green);
        m_mock->mockScreenPixmap = QPixmap::fromImage(img);

        QPixmap result = m_capture->captureFullScreen();
        QCOMPARE(result.size(), QSize(2560, 1440));
    }

    void test_captureFullScreen_withNullPixmap()
    {
        m_mock->mockScreenPixmap = QPixmap();
        QPixmap result = m_capture->captureFullScreen();
        QVERIFY(result.isNull());
    }

    void test_multipleCapturesCalls()
    {
        QSignalSpy spy(m_capture.get(), &ScreenCapture::captureCompleted);

        m_capture->captureFullScreen();
        m_capture->captureRegion(QRect(0, 0, 50, 50));
        m_capture->captureFullScreen();

        QCOMPARE(spy.count(), 3);
    }

private:
    std::unique_ptr<MockPlatformApi> m_mock;
    std::unique_ptr<ScreenCapture> m_capture;
};

QTEST_MAIN(TestScreenCapture)
#include "test_screen_capture.moc"
