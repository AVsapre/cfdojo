#pragma once

#include "CompilationUtils.h"

#include <QString>
#include <QStringList>

// Shared compilation and language configuration used by ExecutionController,
// ParallelExecutor, and MainWindow.
struct CompilationConfig {
    QString language = "C++";
    QString cppCompilerPath = "g++";
    QString cppCompilerFlags = "-O2 -std=c++17";
    QString pythonPath = "python3";
    QString pythonArgs;
    QString javaCompilerPath = "javac";
    QString javaRunPath = "java";
    QString javaArgs;
    bool transcludeTemplate = false;
    QString templateCode{CompilationUtils::kDefaultTemplateCode};
};
