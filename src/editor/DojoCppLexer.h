#pragma once

#include <Qsci/qscilexercpp.h>

class DojoCppLexer : public QsciLexerCPP {
    Q_OBJECT

public:
    explicit DojoCppLexer(QObject *parent = nullptr);

    const char *keywords(int set) const override;

private:
    QByteArray keywords2_;
};
