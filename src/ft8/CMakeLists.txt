add_library(FT8 STATIC
    qso.cpp worker.c utils.c gfsk.c
    table_view.c
    cq_scheduler.c
    tx_worker.c
    audio_worker.c
    hound.c
)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/..")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../qth")

target_link_libraries(FT8 PUBLIC aether_x6100_control)

# Idiomas: la tabla de FT8 dibuja textos (i18n_hook.h)
set_source_files_properties(table_view.c PROPERTIES COMPILE_OPTIONS "-include;${CMAKE_CURRENT_SOURCE_DIR}/../i18n_hook.h")
