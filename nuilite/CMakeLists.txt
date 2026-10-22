project(nuilite)

find_path(LIBCAIRO_INC NAME cairo.h PATHS /usr/include/cairo /usr/local/include/cairo /opt/homebrew/include/cairo)
find_library(LIBCAIRO NAME cairo PATHS /usr/local/lib /usr/lib /opt/homebrew/lib)

find_path(LIBFT_INC NAME ft2build.h PATHS /usr/include/freetype2 /usr/local/include/freetype2 /opt/homebrew/include/freetype2)
find_library(LIBFT NAME libfreetype PATHS /usr/local/lib /usr/lib /opt/homebrew/lib)

find_library(LIBGPIOD NAME gpiod PATHS /usr/local/lib /usr/lib /lib /usr/lib/arm-linux-gnueabihf)

# Default screen driver: ssd1325 (Fates) or ssd1322 (stock shield)
if (NOT DEFINED DEFAULT_SCREEN_DRIVER)
    set(DEFAULT_SCREEN_DRIVER "ssd1325" CACHE STRING "Default screen driver: ssd1325 or ssd1322")
endif()
add_definitions(-DDEFAULT_SCREEN_DRIVER="${DEFAULT_SCREEN_DRIVER}")

include_directories(
    ${LIBCAIRO_INC}
    ${LIBFT_INC}
    "${PROJECT_SOURCE_DIR}/../external/moodycamel"
    "${PROJECT_SOURCE_DIR}/../external/cJSON"
)

set(FL_H NuiDevice.h NuiScreen.h NuiScreenSSD1325.h NuiScreenSSD1322.h)

if (UNIX AND NOT APPLE)
    set(FL_SRC NuiDevice.cpp NuiScreen.cpp NuiScreenSSD1325.cpp NuiScreenSSD1322.cpp)
else ()
    set(FL_SRC NuiDeviceNop.cpp)
endif()

add_library(nuilite SHARED ${FL_SRC} ${FL_H})

if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
    target_link_libraries(nuilite atomic)
endif ()

target_link_libraries(nuilite "dl" "pthread" cjson ${LIBCAIRO})
if (LIBGPIOD)
    target_link_libraries(nuilite ${LIBGPIOD})
else()
    target_link_libraries(nuilite gpiod)
endif()

target_include_directories(nuilite PUBLIC
        .
        )

add_subdirectory(tests)
