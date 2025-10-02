# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/main/*.*)
idf_component_register(SRCS ${app_sources})

# idf_component_register(
#     SRCS
#     main.c
#     esp/esp_bsp.c
#     esp/esp_lcd_axs15231b.c
#     esp/esp_lcd_touch.c
#     screens/config_screen.c
#     screens/streamdeck_screen.c
#     screens/utils/buttons.c
#     screens/utils/styles.c

#     # Estas são as pastas que contêm os arquivos de cabeçalho (.h)
#     INCLUDE_DIRS
#     .
#     esp
#     screens
#     screens/utils
# )
