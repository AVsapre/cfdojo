#pragma once

#include <QByteArray>
#include <QMap>
#include <QString>

// Handler for .cpack files (Competitive Programming Pack - ZIP with STORE compression)
// Format is a standard ZIP that can be opened with any unzip tool
// Structure:
//   manifest.json   - Version and format info
//   solution.cpp    - User's solution code (the main logic)
//   template.cpp    - Template with //#main transclusion marker
//   problem.json    - Problem metadata from Competitive Companion
//   testcases.json  - Array of input/output test cases
//
// Transclusion: When compiling, //#main in template.cpp is replaced with solution.cpp content
// Default template is just "//#main" (solution is the complete code)
class CpackFileHandler {
public:
    CpackFileHandler() = default;

    // Add a file to the archive (call before save)
    void addFile(const QString &name, const QByteArray &content);
    
    // Get a file from the archive (call after load)
    QByteArray getFile(const QString &name) const;
    
    // Check if a file exists in the archive
    bool hasFile(const QString &name) const;
    
    // Get list of all files in the archive
    QStringList fileNames() const;
    
    // Clear all files
    void clear();

    // Save archive to disk
    bool save(const QString &path) const;
    
    // Load archive from disk
    bool load(const QString &path);
    
    // Error message from last operation
    QString errorString() const { return errorString_; }

private:
    struct LocalFileHeader {
        quint32 signature;
        quint16 versionNeeded;
        quint16 flags;
        quint16 compression;
        quint16 modTime;
        quint16 modDate;
        quint32 crc32;
        quint32 compressedSize;
        quint32 uncompressedSize;
        quint16 fileNameLength;
        quint16 extraFieldLength;
    };

    struct CentralDirHeader {
        quint32 signature;
        quint16 versionMade;
        quint16 versionNeeded;
        quint16 flags;
        quint16 compression;
        quint16 modTime;
        quint16 modDate;
        quint32 crc32;
        quint32 compressedSize;
        quint32 uncompressedSize;
        quint16 fileNameLength;
        quint16 extraFieldLength;
        quint16 commentLength;
        quint16 diskStart;
        quint16 internalAttr;
        quint32 externalAttr;
        quint32 localHeaderOffset;
    };

    struct EndOfCentralDir {
        quint32 signature;
        quint16 diskNumber;
        quint16 diskWithCentralDir;
        quint16 entriesOnDisk;
        quint16 totalEntries;
        quint32 centralDirSize;
        quint32 centralDirOffset;
        quint16 commentLength;
    };

    static constexpr int kManifestVersion = 1;
    static constexpr const char *kManifestFile = "manifest.json";
    
    static quint32 calculateCrc32(const QByteArray &data);
    QString createManifest() const;

    QMap<QString, QByteArray> files_;
    mutable QString errorString_;
};
