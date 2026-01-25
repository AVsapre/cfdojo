#include "editor/DojoCppLexer.h"

#include <QStringList>

DojoCppLexer::DojoCppLexer(QObject *parent)
    : QsciLexerCPP(parent) {
    const QString base = QString::fromLatin1(QsciLexerCPP::keywords(2));
    QStringList combined = base.split(' ', Qt::SkipEmptyParts);

    const QStringList extras = {
        "array",
        "bitset",
        "deque",
        "forward_list",
        "initializer_list",
        "list",
        "map",
        "multimap",
        "multiset",
        "optional",
        "pair",
        "priority_queue",
        "queue",
        "set",
        "stack",
        "string",
        "string_view",
        "tuple",
        "unordered_map",
        "unordered_multimap",
        "unordered_multiset",
        "unordered_set",
        "vector"
    };

    combined.append(extras);
    combined.removeDuplicates();
    combined.sort();

    keywords2_ = combined.join(' ').toLatin1();
}

const char *DojoCppLexer::keywords(int set) const {
    if (set == 2 && !keywords2_.isEmpty()) {
        return keywords2_.constData();
    }
    return QsciLexerCPP::keywords(set);
}
