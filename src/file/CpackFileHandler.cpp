#include "file/CpackFileHandler.h"

#include <QDataStream>
#include <QFile>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace {
constexpr quint32 kLocalFileSignature = 0x04034b50;
constexpr quint32 kCentralDirSignature = 0x02014b50;
constexpr quint32 kEndOfCentralDirSignature = 0x06054b50;
constexpr quint16 kVersionNeeded = 10;  // 1.0 for STORE
constexpr quint16 kVersionMade = 20;    // 2.0
constexpr quint16 kCompressionStore = 0;

// Sanitize filename to prevent path traversal attacks
// Returns empty string if filename is invalid/malicious
QString sanitizeFilename(const QString &filename) {
    if (filename.isEmpty()) {
        return QString();
    }
    
    // Reject absolute paths
    if (filename.startsWith('/') || filename.startsWith('\\')) {
        return QString();
    }
    
    // Reject Windows-style absolute paths (e.g., C:\)
    if (filename.length() >= 2 && filename[1] == ':') {
        return QString();
    }
    
    // Split into components and filter out dangerous ones
    QStringList parts = filename.split(QRegularExpression("[/\\\\]"), Qt::SkipEmptyParts);
    QStringList safeParts;
    
    for (const QString &part : parts) {
        // Skip empty, "." and ".." components
        if (part.isEmpty() || part == "." || part == "..") {
            continue;
        }
        
        // Reject any component that starts with a dot (hidden files can be dangerous)
        // Exception: allow normal dotfiles like ".gitignore" but not ".." or "..."
        if (part.startsWith("..")) {
            return QString();  // Reject any ".." prefix
        }
        
        safeParts.append(part);
    }
    
    if (safeParts.isEmpty()) {
        return QString();
    }
    
    // Reconstruct with forward slashes only
    return safeParts.join('/');
}

// CRC-32 lookup table
const quint32 kCrc32Table[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c2, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7a03, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};
} // namespace

quint32 CpackFileHandler::calculateCrc32(const QByteArray &data) {
    quint32 crc = 0xFFFFFFFF;
    for (char byte : data) {
        crc = kCrc32Table[(crc ^ static_cast<quint8>(byte)) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

QString CpackFileHandler::createManifest() const {
    QJsonObject manifest;
    manifest["version"] = kManifestVersion;
    manifest["format"] = "cfdojo-cpack";
    
    QJsonDocument doc(manifest);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
}

void CpackFileHandler::addFile(const QString &name, const QByteArray &content) {
    files_[name] = content;
}

QByteArray CpackFileHandler::getFile(const QString &name) const {
    return files_.value(name);
}

bool CpackFileHandler::hasFile(const QString &name) const {
    return files_.contains(name);
}

QStringList CpackFileHandler::fileNames() const {
    return files_.keys();
}

void CpackFileHandler::clear() {
    files_.clear();
}

bool CpackFileHandler::save(const QString &path) const {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        errorString_ = file.errorString();
        return false;
    }

    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);

    // Track offsets for central directory (name, offset, content)
    struct FileEntry {
        QString name;
        quint32 offset;
        QByteArray content;
    };
    QList<FileEntry> fileEntries;

    // Write manifest first
    {
        const QString manifestContent = createManifest();
        const QByteArray manifestBytes = manifestContent.toUtf8();
        const QString manifestName = kManifestFile;
        const QByteArray nameBytes = manifestName.toUtf8();
        const quint32 crc = calculateCrc32(manifestBytes);

        fileEntries.append({manifestName, static_cast<quint32>(file.pos()), manifestBytes});

        stream << kLocalFileSignature;
        stream << kVersionNeeded;
        stream << quint16(0);  // flags
        stream << kCompressionStore;
        stream << quint16(0);  // mod time
        stream << quint16(0);  // mod date
        stream << crc;
        stream << quint32(manifestBytes.size());
        stream << quint32(manifestBytes.size());
        stream << quint16(nameBytes.size());
        stream << quint16(0);  // extra field length

        file.write(nameBytes);
        file.write(manifestBytes);
    }

    // Write local file headers and data
    for (auto it = files_.constBegin(); it != files_.constEnd(); ++it) {
        const QString &name = it.key();
        const QByteArray &content = it.value();
        const QByteArray nameBytes = name.toUtf8();
        const quint32 crc = calculateCrc32(content);

        fileEntries.append({name, static_cast<quint32>(file.pos()), content});

        // Local file header
        stream << kLocalFileSignature;
        stream << kVersionNeeded;
        stream << quint16(0);  // flags
        stream << kCompressionStore;
        stream << quint16(0);  // mod time
        stream << quint16(0);  // mod date
        stream << crc;
        stream << quint32(content.size());  // compressed size
        stream << quint32(content.size());  // uncompressed size
        stream << quint16(nameBytes.size());
        stream << quint16(0);  // extra field length

        // File name
        file.write(nameBytes);

        // File data (uncompressed)
        file.write(content);
    }

    // Central directory
    const quint32 centralDirOffset = static_cast<quint32>(file.pos());

    for (const auto &entry : fileEntries) {
        const QByteArray nameBytes = entry.name.toUtf8();
        const quint32 crc = calculateCrc32(entry.content);

        stream << kCentralDirSignature;
        stream << kVersionMade;
        stream << kVersionNeeded;
        stream << quint16(0);  // flags
        stream << kCompressionStore;
        stream << quint16(0);  // mod time
        stream << quint16(0);  // mod date
        stream << crc;
        stream << quint32(entry.content.size());  // compressed size
        stream << quint32(entry.content.size());  // uncompressed size
        stream << quint16(nameBytes.size());
        stream << quint16(0);  // extra field length
        stream << quint16(0);  // comment length
        stream << quint16(0);  // disk start
        stream << quint16(0);  // internal attributes
        stream << quint32(0);  // external attributes
        stream << entry.offset;

        file.write(nameBytes);
    }

    const quint32 centralDirSize = static_cast<quint32>(file.pos()) - centralDirOffset;
    const quint16 totalEntries = static_cast<quint16>(fileEntries.size());

    // End of central directory
    stream << kEndOfCentralDirSignature;
    stream << quint16(0);  // disk number
    stream << quint16(0);  // disk with central dir
    stream << totalEntries;
    stream << totalEntries;
    stream << centralDirSize;
    stream << centralDirOffset;
    stream << quint16(0);  // comment length

    file.close();
    return true;
}

bool CpackFileHandler::load(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        errorString_ = file.errorString();
        return false;
    }

    files_.clear();
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);

    while (!file.atEnd()) {
        quint32 signature;
        stream >> signature;

        if (signature == kLocalFileSignature) {
            quint16 versionNeeded, flags, compression, modTime, modDate;
            quint32 crc, compressedSize, uncompressedSize;
            quint16 fileNameLength, extraFieldLength;

            stream >> versionNeeded >> flags >> compression;
            stream >> modTime >> modDate;
            stream >> crc >> compressedSize >> uncompressedSize;
            stream >> fileNameLength >> extraFieldLength;

            QByteArray fileName = file.read(fileNameLength);
            file.skip(extraFieldLength);

            if (compression != kCompressionStore) {
                errorString_ = "Only STORE compression is supported";
                return false;
            }

            QByteArray content = file.read(compressedSize);
            
            // Sanitize filename to prevent path traversal attacks
            QString safeName = sanitizeFilename(QString::fromUtf8(fileName));
            if (safeName.isEmpty()) {
                errorString_ = QString("Invalid filename in archive: %1").arg(QString::fromUtf8(fileName));
                return false;
            }
            
            files_[safeName] = content;

        } else if (signature == kCentralDirSignature) {
            // Skip central directory entry
            quint16 versionMade, versionNeeded, flags, compression, modTime, modDate;
            quint32 crc, compressedSize, uncompressedSize;
            quint16 fileNameLength, extraFieldLength, commentLength;
            quint16 diskStart, internalAttr;
            quint32 externalAttr, localOffset;

            stream >> versionMade >> versionNeeded >> flags >> compression;
            stream >> modTime >> modDate;
            stream >> crc >> compressedSize >> uncompressedSize;
            stream >> fileNameLength >> extraFieldLength >> commentLength;
            stream >> diskStart >> internalAttr >> externalAttr >> localOffset;

            file.skip(fileNameLength + extraFieldLength + commentLength);

        } else if (signature == kEndOfCentralDirSignature) {
            // End of central directory - we're done
            break;
        } else {
            errorString_ = QString("Unknown ZIP signature: 0x%1").arg(signature, 8, 16, QChar('0'));
            return false;
        }
    }

    file.close();
    return true;
}
