#include "file_saver.h"
#include <QStandardPaths>
#include <QDateTime>
#include <QDir>

namespace easyshotter {

FileSaver::FileSaver(QObject* parent)
    : QObject(parent)
{
}

FileSaver::~FileSaver() = default;

bool FileSaver::saveToFile(const QPixmap& pixmap, const QString& filePath)
{
    return pixmap.save(filePath);
}

bool FileSaver::saveToDesktop(const QPixmap& pixmap, Format format)
{
    QString desktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    return saveToDirectory(pixmap, desktop, format);
}

bool FileSaver::saveToDirectory(const QPixmap& pixmap, const QString& directory,
                                 Format format)
{
    QDir dir(directory);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    QString fileName = generateFileName(format);
    QString filePath = dir.filePath(fileName);
    return pixmap.save(filePath, formatExtension(format));
}

void FileSaver::setDefaultFormat(Format format)
{
    m_defaultFormat = format;
}

FileSaver::Format FileSaver::defaultFormat() const
{
    return m_defaultFormat;
}

QString FileSaver::generateFileName(Format format)
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    return QString("easyshotter_%1.%2").arg(timestamp, formatExtension(format));
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

} // namespace easyshotter
