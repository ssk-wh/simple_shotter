#include "file_saver.h"
#include <QStandardPaths>
#include <QDateTime>
#include <QDir>

namespace simpleshotter {

FileSaver::FileSaver(QObject* parent)
    : QObject(parent)
{
}

FileSaver::~FileSaver() = default;

QString FileSaver::saveToFile(const QPixmap& pixmap, const QString& filePath)
{
    if (pixmap.isNull()) {
        setLastError(QString::fromUtf8("图片为空"));
        return QString();
    }

    if (filePath.isEmpty()) {
        setLastError(QString::fromUtf8("文件路径为空"));
        return QString();
    }

    if (!pixmap.save(filePath)) {
        setLastError(QString::fromUtf8("保存图片失败：%1").arg(filePath));
        return QString();
    }

    m_lastError.clear();
    return filePath;
}

QString FileSaver::saveToDesktop(const QPixmap& pixmap, Format format)
{
    QString desktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    return saveToDirectory(pixmap, desktop, format);
}

QString FileSaver::saveToDirectory(const QPixmap& pixmap, const QString& directory,
                                   Format format)
{
    if (pixmap.isNull()) {
        setLastError(QString::fromUtf8("图片为空"));
        return QString();
    }

    QDir dir(directory);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            setLastError(QString::fromUtf8("无法创建目录：%1").arg(directory));
            return QString();
        }
    }

    QString fileName = generateFileName(format);
    QString filePath = dir.filePath(fileName);

    if (!pixmap.save(filePath, formatExtension(format))) {
        setLastError(QString::fromUtf8("保存图片失败：%1").arg(filePath));
        return QString();
    }

    m_lastError.clear();
    return filePath;
}

void FileSaver::setDefaultFormat(Format format)
{
    m_defaultFormat = format;
}

FileSaver::Format FileSaver::defaultFormat() const
{
    return m_defaultFormat;
}

QString FileSaver::getLastError() const
{
    return m_lastError;
}

void FileSaver::setLastError(const QString& error)
{
    m_lastError = error;
}

QString FileSaver::generateFileName(Format format)
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    return QString("simpleshotter_%1.%2").arg(timestamp, formatExtension(format));
}

const char* FileSaver::formatExtension(Format format)
{
    switch (format) {
    case Format::PNG: return "png";
    case Format::JPG: return "jpg";
    case Format::BMP: return "bmp";
    }
    return "png";
}

} // namespace simpleshotter
