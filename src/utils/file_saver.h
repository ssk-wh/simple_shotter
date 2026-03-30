#pragma once

#include <QObject>
#include <QPixmap>
#include <QString>

namespace simpleshotter {

class FileSaver : public QObject {
    Q_OBJECT
public:
    explicit FileSaver(QObject* parent = nullptr);
    ~FileSaver();

    enum class Format { PNG, JPG, BMP };

    // Returns file path on success, empty string on failure
    QString saveToFile(const QPixmap& pixmap, const QString& filePath);
    QString saveToDesktop(const QPixmap& pixmap, Format format = Format::PNG);
    QString saveToDirectory(const QPixmap& pixmap, const QString& directory,
                            Format format = Format::PNG);

    void setDefaultFormat(Format format);
    Format defaultFormat() const;

    // Get last error message
    QString getLastError() const;

    static QString generateFileName(Format format);
    static const char* formatExtension(Format format);

private:
    void setLastError(const QString& error);

    Format m_defaultFormat = Format::PNG;
    QString m_lastError;
};

} // namespace simpleshotter
