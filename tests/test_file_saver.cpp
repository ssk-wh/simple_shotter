#include <QtTest>
#include <QTemporaryDir>
#include <QImage>
#include <QFileInfo>
#include <QRegularExpression>
#include "utils/file_saver.h"

using namespace easyshotter;

class TestFileSaver : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        m_saver = std::make_unique<FileSaver>();
        m_tempDir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_tempDir->isValid());

        // Prepare a valid test pixmap
        QImage img(200, 150, QImage::Format_ARGB32);
        img.fill(QColor(100, 150, 200));
        m_testPixmap = QPixmap::fromImage(img);
    }

    void cleanup()
    {
        m_saver.reset();
        m_tempDir.reset();
    }

    // --- saveToDirectory ---

    void test_saveToDirectory_png()
    {
        bool ok = m_saver->saveToDirectory(m_testPixmap, m_tempDir->path(),
                                            FileSaver::Format::PNG);
        QVERIFY(ok);

        // Verify a PNG file was created
        QDir dir(m_tempDir->path());
        QStringList files = dir.entryList({"*.png"}, QDir::Files);
        QCOMPARE(files.size(), 1);
    }

    void test_saveToDirectory_jpg()
    {
        bool ok = m_saver->saveToDirectory(m_testPixmap, m_tempDir->path(),
                                            FileSaver::Format::JPG);
        QVERIFY(ok);

        QDir dir(m_tempDir->path());
        QStringList files = dir.entryList({"*.jpg"}, QDir::Files);
        QCOMPARE(files.size(), 1);
    }

    void test_saveToDirectory_bmp()
    {
        bool ok = m_saver->saveToDirectory(m_testPixmap, m_tempDir->path(),
                                            FileSaver::Format::BMP);
        QVERIFY(ok);

        QDir dir(m_tempDir->path());
        QStringList files = dir.entryList({"*.bmp"}, QDir::Files);
        QCOMPARE(files.size(), 1);
    }

    // --- File name contains timestamp ---

    void test_generateFileName_containsTimestamp()
    {
        QString name = FileSaver::generateFileName(FileSaver::Format::PNG);
        // Pattern: easyshotter_YYYYMMDD_HHmmss.png
        QRegularExpression re(R"(easyshotter_\d{8}_\d{6}\.png)");
        QVERIFY2(re.match(name).hasMatch(),
                 qPrintable(QString("Filename '%1' doesn't match expected pattern").arg(name)));
    }

    void test_generateFileName_jpg_extension()
    {
        QString name = FileSaver::generateFileName(FileSaver::Format::JPG);
        QVERIFY(name.endsWith(".jpg"));
    }

    void test_generateFileName_bmp_extension()
    {
        QString name = FileSaver::generateFileName(FileSaver::Format::BMP);
        QVERIFY(name.endsWith(".bmp"));
    }

    // --- File actually exists after save ---

    void test_savedFileExists()
    {
        m_saver->saveToDirectory(m_testPixmap, m_tempDir->path(),
                                  FileSaver::Format::PNG);

        QDir dir(m_tempDir->path());
        QStringList files = dir.entryList({"*.png"}, QDir::Files);
        QCOMPARE(files.size(), 1);

        QFileInfo fi(dir.filePath(files.first()));
        QVERIFY(fi.exists());
        QVERIFY(fi.size() > 0);
    }

    void test_savedFileIsReadable()
    {
        m_saver->saveToDirectory(m_testPixmap, m_tempDir->path(),
                                  FileSaver::Format::PNG);

        QDir dir(m_tempDir->path());
        QStringList files = dir.entryList({"*.png"}, QDir::Files);
        QCOMPARE(files.size(), 1);

        QImage loaded(dir.filePath(files.first()));
        QVERIFY(!loaded.isNull());
        QCOMPARE(loaded.size(), QSize(200, 150));
    }

    // --- saveToFile ---

    void test_saveToFile_directPath()
    {
        QString path = m_tempDir->filePath("test_direct.png");
        bool ok = m_saver->saveToFile(m_testPixmap, path);
        QVERIFY(ok);
        QVERIFY(QFileInfo::exists(path));
    }

    // --- Empty pixmap handling ---

    void test_saveEmptyPixmap_returnsFalse()
    {
        QPixmap empty;
        bool ok = m_saver->saveToDirectory(empty, m_tempDir->path(),
                                            FileSaver::Format::PNG);
        QVERIFY(!ok);
    }

    void test_saveEmptyPixmap_noFileCreated()
    {
        QPixmap empty;
        m_saver->saveToDirectory(empty, m_tempDir->path(), FileSaver::Format::PNG);

        QDir dir(m_tempDir->path());
        QStringList files = dir.entryList({"*.png"}, QDir::Files);
        QCOMPARE(files.size(), 0);
    }

    // --- Non-existent directory: auto-create ---

    void test_saveToNonExistentDir_autoCreates()
    {
        QString subDir = m_tempDir->filePath("sub/nested/dir");
        bool ok = m_saver->saveToDirectory(m_testPixmap, subDir,
                                            FileSaver::Format::PNG);
        QVERIFY(ok);

        QDir dir(subDir);
        QStringList files = dir.entryList({"*.png"}, QDir::Files);
        QCOMPARE(files.size(), 1);
    }

    // --- formatExtension ---

    void test_formatExtension_png()
    {
        QCOMPARE(QString(FileSaver::formatExtension(FileSaver::Format::PNG)), QString("png"));
    }

    void test_formatExtension_jpg()
    {
        QCOMPARE(QString(FileSaver::formatExtension(FileSaver::Format::JPG)), QString("jpg"));
    }

    void test_formatExtension_bmp()
    {
        QCOMPARE(QString(FileSaver::formatExtension(FileSaver::Format::BMP)), QString("bmp"));
    }

    // --- Default format ---

    void test_defaultFormat_isPNG()
    {
        QCOMPARE(m_saver->defaultFormat(), FileSaver::Format::PNG);
    }

    void test_setDefaultFormat()
    {
        m_saver->setDefaultFormat(FileSaver::Format::JPG);
        QCOMPARE(m_saver->defaultFormat(), FileSaver::Format::JPG);
    }

private:
    std::unique_ptr<FileSaver> m_saver;
    std::unique_ptr<QTemporaryDir> m_tempDir;
    QPixmap m_testPixmap;
};

QTEST_MAIN(TestFileSaver)
#include "test_file_saver.moc"
