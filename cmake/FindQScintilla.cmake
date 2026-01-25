find_path(QScintilla_INCLUDE_DIR
    NAMES Qsci/qsciscintilla.h
    PATHS
        /usr/include
        /usr/include/x86_64-linux-gnu
        /usr/local/include
    PATH_SUFFIXES
        qt6
        qt5
)

find_library(QScintilla_LIBRARY
    NAMES qscintilla2_qt6 qscintilla2_qt5 qscintilla2
    PATHS
        /usr/lib
        /usr/lib/x86_64-linux-gnu
        /usr/local/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(QScintilla
    REQUIRED_VARS QScintilla_INCLUDE_DIR QScintilla_LIBRARY
)

if (QScintilla_FOUND AND NOT TARGET QScintilla::QScintilla)
    add_library(QScintilla::QScintilla UNKNOWN IMPORTED)
    set_target_properties(QScintilla::QScintilla PROPERTIES
        IMPORTED_LOCATION "${QScintilla_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${QScintilla_INCLUDE_DIR}"
    )
endif()

set(QScintilla_INCLUDE_DIRS "${QScintilla_INCLUDE_DIR}")
set(QScintilla_LIBRARIES "${QScintilla_LIBRARY}")
