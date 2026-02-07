# VendorDeps.cmake — fetch and build third-party dependencies from source.
#
# Controlled by CFDOJO_VENDOR (ON by default).
#   -DCFDOJO_VENDOR=ON   → download + build QScintilla from source
#   -DCFDOJO_VENDOR=OFF  → use system-installed QScintilla (FindQScintilla)

include(FetchContent)

# ── QScintilla ───────────────────────────────────────────────────────────────
set(QSCINTILLA_VERSION "2.14.1" CACHE STRING "QScintilla version to vendor")
set(QSCINTILLA_URL
    "https://www.riverbankcomputing.com/static/Downloads/QScintilla/${QSCINTILLA_VERSION}/QScintilla_src-${QSCINTILLA_VERSION}.tar.gz"
)

FetchContent_Declare(
    qscintilla_src
    URL      "${QSCINTILLA_URL}"
    URL_HASH ""                      # add SHA256 once pinned
    DOWNLOAD_DIR "${CMAKE_SOURCE_DIR}/vendor/downloads"
    SOURCE_DIR   "${CMAKE_SOURCE_DIR}/vendor/QScintilla"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(qscintilla_src)

# QScintilla ships a qmake project, not CMake.  Build the subset we need
# (the Qt6 widget library) as a static OBJECT library directly from its
# sources so we don't depend on qmake at all.

set(_QS_SRC "${qscintilla_src_SOURCE_DIR}/src")
set(_QS_SCINTILLA "${qscintilla_src_SOURCE_DIR}/scintilla")
set(_QS_LEXLIB "${_QS_SCINTILLA}/lexlib")
set(_QS_LEXERS "${_QS_SCINTILLA}/lexers")
set(_QS_SCI_SRC "${_QS_SCINTILLA}/src")
set(_QS_SCI_INC "${_QS_SCINTILLA}/include")

file(GLOB _QS_WIDGET_SRCS  "${_QS_SRC}/*.cpp")
file(GLOB _QS_LEXLIB_SRCS  "${_QS_LEXLIB}/*.cxx")
file(GLOB _QS_LEXERS_SRCS  "${_QS_LEXERS}/*.cxx")
file(GLOB _QS_SCI_SRCS     "${_QS_SCI_SRC}/*.cxx")

# Remove sources that need Qt6::PrintSupport (not required for CF Dojo)
list(FILTER _QS_WIDGET_SRCS EXCLUDE REGEX "qsciprinter\\.cpp$")

add_library(QScintilla_Vendor STATIC
    ${_QS_WIDGET_SRCS}
    ${_QS_LEXLIB_SRCS}
    ${_QS_LEXERS_SRCS}
    ${_QS_SCI_SRCS}
)

set_target_properties(QScintilla_Vendor PROPERTIES
    AUTOMOC ON
    POSITION_INDEPENDENT_CODE ON
    CXX_STANDARD 17
)

target_compile_definitions(QScintilla_Vendor PRIVATE
    SCINTILLA_QT=1
    SCI_LEXER=1
    QSCINTILLA_MAKE_DLL        # needed even for static builds to get correct export macros
    INCLUDE_DEPRECATED_FEATURES
)

target_include_directories(QScintilla_Vendor
    PUBLIC
        "${_QS_SRC}"               # Qsci/ headers
        "${_QS_SRC}/.."            # so #include <Qsci/...> works
        "${_QS_SCI_INC}"           # Scintilla.h, ILexer.h, etc.
    PRIVATE
        "${_QS_SCI_SRC}"
        "${_QS_LEXLIB}"
)

target_link_libraries(QScintilla_Vendor
    PUBLIC  Qt6::Widgets
    PRIVATE Qt6::Core Qt6::Gui
)

# Provide the same imported target name as FindQScintilla so the rest of
# CMakeLists.txt doesn't need to know which path was taken.
if(NOT TARGET QScintilla::QScintilla)
    add_library(QScintilla::QScintilla ALIAS QScintilla_Vendor)
endif()
