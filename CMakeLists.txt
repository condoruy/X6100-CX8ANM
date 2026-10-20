cmake_minimum_required(VERSION 3.23)

# Require C11 standard globally
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

project(x6100_gui)

# Versión CX8ANM: VERSION_CX8ANM ("1.0"; el workflow de GitHub le suma el número de build)
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/VERSION_CX8ANM CX8ANM_VERSION)
string(STRIP "${CX8ANM_VERSION}" CX8ANM_VERSION)
add_compile_definitions(CX8ANM_VERSION="${CX8ANM_VERSION}")

include_directories(.)

if(ENABLE_TESTING)
        Include(FetchContent)

        FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.4.0 # or a later release
        )

        FetchContent_MakeAvailable(Catch2)

        add_compile_definitions(LV_DRAW_NEON=0)


        # include_directories(${CMAKE_CURRENT_SOURCE_DIR}/tests/mock_gpiod)
        FetchContent_Declare(
        aether_x6100_control
        GIT_REPOSITORY https://github.com/gdyuldin/X6100Control.git
        GIT_TAG        v0.16.1
        )
        FetchContent_MakeAvailable(aether_x6100_control)

else()
        add_compile_options(-mtune=cortex-a7 -mcpu=cortex-a7 -mfloat-abi=hard -mfpu=neon-vfpv4)
        # Unused functions and data are not linked
        add_compile_options(-ffunction-sections -fdata-sections)
        add_link_options(-Wl,--gc-sections)
endif()

add_subdirectory(lvgl)

if(ENABLE_TESTING AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests)
        enable_testing()
        add_subdirectory(tests)
else()
        add_subdirectory(third-party/js8core)
        add_subdirectory(third-party/wsprd)
        add_subdirectory(src)
        add_subdirectory(btcat)
        add_subdirectory(lv_drivers)
        add_subdirectory(sql)
        install(TARGETS ${PROJECT_NAME} DESTINATION sbin)
        install(DIRECTORY rootfs/
                DESTINATION /
                USE_SOURCE_PERMISSIONS
        )
        install(DIRECTORY images DESTINATION share/x6100)
        # Callsign -> country for the FT8/FT4 table (AD1C, country-files.com)
        install(FILES third-party/cty/cty.dat DESTINATION share/x6100)
        # Copied to /mnt/themes at start when missing (main.c)
        install(DIRECTORY themes/RS DESTINATION share/x6100/themes)
        # Temas estilo equipo de alta gama (CX8ANM): themes/alta_gama.py los genera
        install(DIRECTORY themes/alta-gama/ DESTINATION share/x6100/themes)
endif()


