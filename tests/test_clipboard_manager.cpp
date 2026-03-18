#include <QtTest>
#include <QClipboard>
#include <QApplication>
#include <QImage>
#include "utils/clipboard_manager.h"

using namespace simpleshotter;

class TestClipboardManager : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        m_manager = std::make_unique<ClipboardManager>();
    }

    void cleanup()
    {
        m_manager.reset();
    }

    // --- copyToClipboard ---

    void test_copyImage_setsClipboard()
    {
        QImage img(100, 80, QImage::Format_ARGB32);
        img.fill(Qt::red);
        QPixmap pixmap = QPixmap::fromImage(img);

        m_manager->copyToClipboard(pixmap);

        QClipboard* clipboard = QApplication::clipboard();
        QPixmap fromClipboard = clipboard->pixmap();
        QVERIFY(!fromClipboard.isNull());
        QCOMPARE(fromClipboard.size(), QSize(100, 80));
    }

    void test_copyImage_contentMatchesOriginal()
    {
        QImage img(50, 50, QImage::Format_ARGB32);
        img.fill(QColor(128, 64, 32));
        QPixmap pixmap = QPixmap::fromImage(img);

        m_manager->copyToClipboard(pixmap);

        QClipboard* clipboard = QApplication::clipboard();
        QImage fromClipboard = clipboard->pixmap().toImage().convertToFormat(QImage::Format_ARGB32);
        // Check a sample pixel
        QColor pixel = fromClipboard.pixelColor(25, 25);
        QCOMPARE(pixel.red(), 128);
        QCOMPARE(pixel.green(), 64);
        QCOMPARE(pixel.blue(), 32);
    }

    void test_copyEmptyPixmap_doesNotCrash()
    {
        QPixmap empty;
        // Should not crash
        m_manager->copyToClipboard(empty);
        // No assertion on clipboard content for empty pixmap
        // The key point is no crash
    }

    void test_consecutiveCopies_lastWins()
    {
        QImage img1(100, 100, QImage::Format_ARGB32);
        img1.fill(Qt::blue);
        QPixmap pm1 = QPixmap::fromImage(img1);

        QImage img2(200, 150, QImage::Format_ARGB32);
        img2.fill(Qt::green);
        QPixmap pm2 = QPixmap::fromImage(img2);

        m_manager->copyToClipboard(pm1);
        m_manager->copyToClipboard(pm2);

        QPixmap result = QApplication::clipboard()->pixmap();
        QCOMPARE(result.size(), QSize(200, 150));
    }

    void test_copyLargeImage()
    {
        QImage img(3840, 2160, QImage::Format_ARGB32);
        img.fill(Qt::cyan);
        QPixmap pixmap = QPixmap::fromImage(img);

        m_manager->copyToClipboard(pixmap);

        QPixmap result = QApplication::clipboard()->pixmap();
        QVERIFY(!result.isNull());
        QCOMPARE(result.size(), QSize(3840, 2160));
    }

private:
    std::unique_ptr<ClipboardManager> m_manager;
};

QTEST_MAIN(TestClipboardManager)
#include "test_clipboard_manager.moc"
