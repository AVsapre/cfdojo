cmake_minimum_required(VERSION 3.16)
project(CFDojo)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CFDOJO_VENDOR "Vendor third-party dependencies instead of using system packages" ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Concurrent)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

if(CFDOJO_VENDOR)
    include(cmake/VendorDeps.cmake)
else()
    find_package(QScintilla REQUIRED)
endif()

include(CTest)

add_executable(CFDojo
    src/app/ActivityBarButton.cpp
    src/app/ActivityBarButton.h
    src/app/CollapsibleSplitter.cpp
    src/app/CollapsibleSplitter.h
    src/app/MainWindow.cpp
    src/app/MainWindow.h
    src/app/SettingsDialog.cpp
    src/app/SettingsDialog.h
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

if(BUILD_TESTING)
    add_executable(CFDojoCpackTests
        test/test_cpack_handler.cpp
        src/file/CpackFileHandler.cpp
        src/file/CpackFileHandler.h
    )
    target_include_directories(CFDojoCpackTests PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
    target_link_libraries(CFDojoCpackTests PRIVATE Qt6::Core)
    add_test(NAME cpack_handler COMMAND CFDojoCpackTests)
endif()
