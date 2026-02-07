#include "file/CpackFileHandler.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <cstdlib>

namespace {

bool check(bool condition, const QString &message) {
    if (!condition) {
        qCritical().noquote() << message;
        return false;
    }
    return true;
}

QByteArray buildSingleFileZip(const QByteArray &fileName, const QByteArray &content) {
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    constexpr quint32 kLocalFileSignature = 0x04034b50;
    constexpr quint32 kEndOfCentralDirSignature = 0x06054b50;

    stream << kLocalFileSignature;
    stream << quint16(10);  // version needed
    stream << quint16(0);   // flags
    stream << quint16(0);   // compression (STORE)
    stream << quint16(0);   // mod time
    stream << quint16(0);   // mod date
    stream << quint32(0);   // crc32 (not checked by loader)
    stream << quint32(content.size());
    stream << quint32(content.size());
    stream << quint16(fileName.size());
    stream << quint16(0);   // extra field length
    stream.writeRawData(fileName.constData(), fileName.size());
    stream.writeRawData(content.constData(), content.size());

    stream << kEndOfCentralDirSignature;
    stream << quint16(0);
    stream << quint16(0);
    stream << quint16(0);
    stream << quint16(0);
    stream << quint32(0);
    stream << quint32(0);
    stream << quint16(0);

    return bytes;
}

bool testSaveLoadRoundTrip() {
    QTemporaryDir tempDir;
    if (!check(tempDir.isValid(), "Failed to create temporary directory")) {
        return false;
    }

    const QString path = tempDir.filePath("roundtrip.cpack");
    const QByteArray solution = "int main() { return 0; }\n";
    const QByteArray tmpl = "//#main\n";

    CpackFileHandler writer;
    writer.addFile("solution.cpp", solution);
    writer.addFile("template.cpp", tmpl);
    writer.addFile("testcases.json", "{\"tests\":[],\"timeout\":5}");
    if (!check(writer.save(path), "Failed to save archive: " + writer.errorString())) {
        return false;
    }

    CpackFileHandler reader;
    if (!check(reader.load(path), "Failed to load archive: " + reader.errorString())) {
        return false;
    }

    if (!check(reader.hasFile("manifest.json"), "manifest.json missing after load")) {
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument manifestDoc =
        QJsonDocument::fromJson(reader.getFile("manifest.json"), &parseError);
    if (!check(parseError.error == QJsonParseError::NoError && manifestDoc.isObject(),
               "manifest.json is not valid JSON")) {
        return false;
    }
    const QJsonObject manifest = manifestDoc.object();
    if (!check(manifest.value("format").toString() == "cfdojo-cpack",
               "Unexpected manifest format")) {
        return false;
    }
    if (!check(manifest.value("version").toInt() == 1,
               "Unexpected manifest version")) {
        return false;
    }
    if (!check(reader.getFile("solution.cpp") == solution,
               "solution.cpp mismatch after round trip")) {
        return false;
    }
    if (!check(reader.getFile("template.cpp") == tmpl,
               "template.cpp mismatch after round trip")) {
        return false;
    }

    return true;
}

bool testRejectsAbsoluteFilename() {
    QTemporaryDir tempDir;
    if (!check(tempDir.isValid(), "Failed to create temporary directory")) {
        return false;
    }

    const QString path = tempDir.filePath("invalid_filename.cpack");
    const QByteArray archive = buildSingleFileZip("/evil.cpp", "int main() {}\n");

    QFile file(path);
    if (!check(file.open(QIODevice::WriteOnly), "Failed to write crafted archive")) {
        return false;
    }
    file.write(archive);
    file.close();

    CpackFileHandler reader;
    if (!check(!reader.load(path), "Expected load to fail for absolute filename")) {
        return false;
    }
    if (!check(reader.errorString().contains("Invalid filename"),
               "Expected invalid filename error message")) {
        return false;
    }

    return true;
}

}  // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);

    bool ok = true;
    ok = testSaveLoadRoundTrip() && ok;
    ok = testRejectsAbsoluteFilename() && ok;

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
