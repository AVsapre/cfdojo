cmake_minimum_required(VERSION 3.16)
project(CFDojo)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Concurrent)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
find_package(QScintilla REQUIRED)

add_executable(CFDojo
    src/app/ActivityBarButton.cpp
    src/app/ActivityBarButton.h
    src/app/CollapsibleSplitter.cpp
    src/app/CollapsibleSplitter.h
    src/app/HomeWindow.cpp
    src/app/HomeWindow.h
    src/app/LandingButton.cpp
    src/app/LandingButton.h
    src/app/MainWindow.cpp
    src/app/MainWindow.h
    src/app/SettingsDialog.cpp
    src/app/SettingsDialog.h
    src/app/TrainerWindow.cpp
    src/app/TrainerWindow.h
    src/editor/EditorConfigurator.cpp
    src/editor/EditorConfigurator.h
    src/editor/EditorPlaceholder.cpp
    src/editor/EditorPlaceholder.h
    src/editor/DojoCppLexer.cpp
    src/editor/DojoCppLexer.h
    src/execution/ExecutionController.cpp
    src/execution/ExecutionController.h
    src/execution/ParallelExecutor.cpp
    src/execution/ParallelExecutor.h
    src/file/CpackFileHandler.cpp
    src/file/CpackFileHandler.h
    src/companion/CompanionListener.cpp
    src/companion/CompanionListener.h
    src/main.cpp
    src/ui/AutoResizingTextEdit.cpp
    src/ui/AutoResizingTextEdit.h
    src/ui/FileExplorerBuilder.cpp
    src/ui/FileExplorerBuilder.h
    src/ui/StressPanelBuilder.cpp
    src/ui/StressPanelBuilder.h
    src/ui/TestPanelBuilder.cpp
    src/ui/TestPanelBuilder.h
    src/theme/ThemeManager.cpp
    src/theme/ThemeManager.h
    src/resource.qrc
)

target_include_directories(CFDojo PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)

target_link_libraries(CFDojo PRIVATE Qt6::Widgets Qt6::Network Qt6::Concurrent QScintilla::QScintilla)
