project(cJSON)

add_library(cjson STATIC
        cJSON.c
        cJSON.h
        )
set_target_properties(cjson PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(cjson PUBLIC .)
set_target_properties(cjson PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS true)
