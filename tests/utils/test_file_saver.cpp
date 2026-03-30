#include <gtest/gtest.h>
#include "utils/file_saver.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QPixmap>
#include <QImage>

using namespace simpleshotter;

class FileSaverTest : public ::testing::Test {
protected:
    FileSaver fileSaver;
    QPixmap testPixmap;
    QTemporaryDir tempDir;

    void SetUp() override {
        // Create a 100x100 red test pixmap
        QImage img(100, 100, QImage::Format_ARGB32);
        img.fill(Qt::red);
        testPixmap = QPixmap::fromImage(img);
        ASSERT_FALSE(testPixmap.isNull());
    }

    void TearDown() override {
        // Temporary directory auto-cleans on destructor
    }
};

// Test saveToFile returns path on success
TEST_F(FileSaverTest, SaveToFileReturnsPathOnSuccess) {
    QString filePath = tempDir.path() + "/test.png";
    QString result = fileSaver.saveToFile(testPixmap, filePath);

    EXPECT_EQ(result, filePath);
    EXPECT_TRUE(QFile::exists(filePath));
    EXPECT_TRUE(fileSaver.getLastError().isEmpty());
}

// Test saveToFile with null pixmap
TEST_F(FileSaverTest, SaveToFileReturnsEmptyOnNullPixmap) {
    QPixmap nullPixmap;
    QString filePath = tempDir.path() + "/test.png";
    QString result = fileSaver.saveToFile(nullPixmap, filePath);

    EXPECT_TRUE(result.isEmpty());
    EXPECT_FALSE(fileSaver.getLastError().isEmpty());
}

// Test saveToFile with empty path
TEST_F(FileSaverTest, SaveToFileReturnsEmptyOnEmptyPath) {
    QString result = fileSaver.saveToFile(testPixmap, "");

    EXPECT_TRUE(result.isEmpty());
    EXPECT_FALSE(fileSaver.getLastError().isEmpty());
}

// Test saveToDesktop returns valid path
TEST_F(FileSaverTest, SaveToDesktopReturnsValidPath) {
    QString result = fileSaver.saveToDesktop(testPixmap, FileSaver::Format::PNG);

    EXPECT_FALSE(result.isEmpty());
    EXPECT_TRUE(result.endsWith(".png"));
    EXPECT_TRUE(QFile::exists(result));
    EXPECT_TRUE(fileSaver.getLastError().isEmpty());

    // Cleanup
    QFile::remove(result);
}

// Test saveToDirectory returns valid path
TEST_F(FileSaverTest, SaveToDirectoryReturnsValidPath) {
    QString result = fileSaver.saveToDirectory(testPixmap, tempDir.path(), FileSaver::Format::PNG);

    EXPECT_FALSE(result.isEmpty());
    EXPECT_TRUE(result.contains(tempDir.path()));
    EXPECT_TRUE(result.endsWith(".png"));
    EXPECT_TRUE(QFile::exists(result));
    EXPECT_TRUE(fileSaver.getLastError().isEmpty());
}

// Test saveToDirectory creates directory if not exists
TEST_F(FileSaverTest, SaveToDirectoryCreatesDirectoryIfNotExists) {
    QString newDirPath = tempDir.path() + "/subdir/nested";
    QString result = fileSaver.saveToDirectory(testPixmap, newDirPath, FileSaver::Format::PNG);

    EXPECT_FALSE(result.isEmpty());
    EXPECT_TRUE(QDir(newDirPath).exists());
    EXPECT_TRUE(QFile::exists(result));
}

// Test file name contains timestamp
TEST_F(FileSaverTest, GenerateFileNameContainsTimestamp) {
    QString fileName = FileSaver::generateFileName(FileSaver::Format::PNG);

    EXPECT_TRUE(fileName.startsWith("simpleshotter_"));
    EXPECT_TRUE(fileName.endsWith(".png"));
    // "simpleshotter_YYYYMMDD_HHMMSS.png" length should be >= 26
    EXPECT_GE(fileName.length(), 26);

    // Verify timestamp format by counting underscores
    int underscoreCount = 0;
    for (int i = 0; i < fileName.length(); ++i) {
        if (fileName[i] == '_') underscoreCount++;
    }
    EXPECT_EQ(underscoreCount, 2); // simpleshotter_YYYYMMDD_HHMMSS
}

// Test format extension return correct values
TEST_F(FileSaverTest, FormatExtensionReturnsCorrectValues) {
    EXPECT_STREQ(FileSaver::formatExtension(FileSaver::Format::PNG), "png");
    EXPECT_STREQ(FileSaver::formatExtension(FileSaver::Format::JPG), "jpg");
    EXPECT_STREQ(FileSaver::formatExtension(FileSaver::Format::BMP), "bmp");
}

// Test set default format and retrieve
TEST_F(FileSaverTest, SetDefaultFormatAndRetrieve) {
    fileSaver.setDefaultFormat(FileSaver::Format::JPG);
    EXPECT_EQ(fileSaver.defaultFormat(), FileSaver::Format::JPG);

    fileSaver.setDefaultFormat(FileSaver::Format::BMP);
    EXPECT_EQ(fileSaver.defaultFormat(), FileSaver::Format::BMP);

    fileSaver.setDefaultFormat(FileSaver::Format::PNG);
    EXPECT_EQ(fileSaver.defaultFormat(), FileSaver::Format::PNG);
}

// Test error message cleared on success
TEST_F(FileSaverTest, ErrorMessageClearedOnSuccess) {
    QString filePath = tempDir.path() + "/test.png";

    // First trigger an error
    fileSaver.saveToFile(QPixmap(), filePath);
    EXPECT_FALSE(fileSaver.getLastError().isEmpty());

    // Then save successfully
    QString result = fileSaver.saveToFile(testPixmap, filePath);

    EXPECT_FALSE(result.isEmpty());
    EXPECT_TRUE(fileSaver.getLastError().isEmpty()); // Error should be cleared
}

// Test saveToDirectory with null pixmap
TEST_F(FileSaverTest, SaveToDirectoryHandlesNullPixmap) {
    QPixmap nullPixmap;
    QString result = fileSaver.saveToDirectory(nullPixmap, tempDir.path(), FileSaver::Format::PNG);

    EXPECT_TRUE(result.isEmpty());
    EXPECT_FALSE(fileSaver.getLastError().isEmpty());
}

// Test multiple saves with different formats
TEST_F(FileSaverTest, SaveMultipleFormats) {
    QString pngResult = fileSaver.saveToDirectory(testPixmap, tempDir.path(), FileSaver::Format::PNG);
    QString jpgResult = fileSaver.saveToDirectory(testPixmap, tempDir.path(), FileSaver::Format::JPG);
    QString bmpResult = fileSaver.saveToDirectory(testPixmap, tempDir.path(), FileSaver::Format::BMP);

    EXPECT_FALSE(pngResult.isEmpty());
    EXPECT_FALSE(jpgResult.isEmpty());
    EXPECT_FALSE(bmpResult.isEmpty());

    EXPECT_TRUE(QFile::exists(pngResult));
    EXPECT_TRUE(QFile::exists(jpgResult));
    EXPECT_TRUE(QFile::exists(bmpResult));

    EXPECT_TRUE(pngResult.endsWith(".png"));
    EXPECT_TRUE(jpgResult.endsWith(".jpg"));
    EXPECT_TRUE(bmpResult.endsWith(".bmp"));
}

// Test saveToFile with valid format extension
TEST_F(FileSaverTest, SaveToFilePreservesFileExtension) {
    QString filePath = tempDir.path() + "/custom_name.png";
    QString result = fileSaver.saveToFile(testPixmap, filePath);

    EXPECT_EQ(result, filePath);
    EXPECT_TRUE(QFile::exists(filePath));
}
