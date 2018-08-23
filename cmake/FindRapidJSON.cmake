if (NOT RAPIDJSON_ROOT)
    if (WIN32)
        set(RAPIDJSON_ROOT "C:/Program Files(x86)/RapidJSON" CACHE PATH "Location of RapidJSON")
    else ()
        set(RAPIDJSON_ROOT "/usr" CACHE PATH "Location of RapidJSON")
    endif ()
endif ()

find_path(RAPIDJSON_INCLUDE_PATH
    NAMES rapidjson/rapidjson.h
    PATHS ${RAPIDJSON_ROOT}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(RAPID_JSON
    REQUIRED_VARS RAPIDJSON_INCLUDE_PATH
)