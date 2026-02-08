#pragma once

#include <QProcess>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include <array>

namespace CompilationUtils {

inline constexpr QLatin1StringView kDefaultTemplateCode{"//#main"};

inline const std::array<QString, 3> &supportedLanguages() {
    static const std::array<QString, 3> langs = {
        QStringLiteral("C++"),
        QStringLiteral("Python"),
        QStringLiteral("Java")
    };
    return langs;
}

inline QString normalizeLanguage(const QString &language) {
    const QString normalized = language.trimmed().toLower();
    if (normalized == "python" || normalized == "py") {
        return QStringLiteral("Python");
    }
    if (normalized == "java") {
        return QStringLiteral("Java");
    }
    return QStringLiteral("C++");
}

inline QStringList splitArgs(const QString &args) {
    const QString trimmed = args.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }
    return QProcess::splitCommand(trimmed);
}

inline QString detectJavaMainClass(const QString &code) {
    const QRegularExpression publicClassPattern(
        "\\bpublic\\s+class\\s+([A-Za-z_][A-Za-z0-9_]*)\\b");
    QRegularExpressionMatch match = publicClassPattern.match(code);
    if (match.hasMatch()) {
        return match.captured(1);
    }
    const QRegularExpression classPattern(
        "\\bclass\\s+([A-Za-z_][A-Za-z0-9_]*)\\b");
    match = classPattern.match(code);
    if (match.hasMatch()) {
        return match.captured(1);
    }
    return QStringLiteral("Solution");
}

inline QString applyTransclusion(const QString &templateCode,
                                  const QString &solution,
                                  bool transclude) {
    if (!transclude) {
        return solution;
    }
    if (templateCode.contains("//#main")) {
        QString result = templateCode;
        return result.replace("//#main", solution);
    }
    return solution;
}

inline QString normalizeText(const QString &text) {
    QString normalized = text;
    normalized.replace("\r\n", "\n");
    normalized.replace("\r", "\n");
    QStringList lines = normalized.split('\n');
    while (!lines.isEmpty() && lines.last().trimmed().isEmpty()) {
        lines.removeLast();
    }
    for (QString &line : lines) {
        while (line.endsWith(' ') || line.endsWith('\t')) {
            line.chop(1);
        }
    }
    return lines.join('\n');
}

} // namespace CompilationUtils
