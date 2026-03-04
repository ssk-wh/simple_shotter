#pragma once

#include <QObject>
#include <QPixmap>
#include <QString>

namespace easyshotter {

class FileSaver : public QObject {
    Q_OBJECT
public:
    explicit FileSaver(QObject* parent = nullptr);
    ~FileSaver();

    enum class Format { PNG, JPG, BMP };

    bool saveToFile(const QPixmap& pixmap, const QString& filePath);
    bool saveToDesktop(const QPixmap& pixmap, Format format = Format::PNG);
    bool saveToDirectory(const QPixmap& pixmap, const QString& directory,
                         Format format = Format::PNG);

    void setDefaultFormat(Format format);
    Format defaultFormat() const;

    static QString generateFileName(Format format);
    static const char* formatExtension(Format format);

private:
    Format m_defaultFormat = Format::PNG;
};

} // namespace easyshotter
