if (NOT NUKE_VERSION)
    set(NUKE_VERSION "10.5v7" CACHE STRING "Version string of nuke")
endif()

if (NOT NUKE_ROOT)
    if (WIN32)
        set(NUKE_ROOT "C:/Program Files/Nuke${NUKE_VERSION}" CACHE PATH "Location of nuke")
    else ()
        set(NUKE_ROOT "/usr/local/Nuke${NUKE_VERSION}" CACHE PATH "Location of nuke")
    endif()
endif()

find_path(NUKE_INCLUDE_PATH
    NAMES DDImage/Iop.h
    PATHS ${NUKE_ROOT}/include
)

find_library(NUKE_LIBRARY_PATH
    NAMES DDImage
    PATHS ${NUKE_ROOT}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(NUKE
    REQUIRED_VARS NUKE_INCLUDE_PATH NUKE_LIBRARY_PATH)