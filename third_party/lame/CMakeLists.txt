# LAME MP3 encoder (libmp3lame 3.100, LGPL) - build vendorizado solo codificador.
# Lista de fuentes tomada del build gyp de Chromium (sin mpglib ni SIMD x86;
# lame usa rutas C puras cuando HAVE_XMMINTRIN_H no esta definido).
add_library(mp3lame STATIC
    src/VbrTag.c
    src/bitstream.c
    src/encoder.c
    src/fft.c
    src/gain_analysis.c
    src/id3tag.c
    src/lame.c
    src/newmdct.c
    src/presets.c
    src/psymodel.c
    src/quantize.c
    src/quantize_pvt.c
    src/reservoir.c
    src/set_get.c
    src/tables.c
    src/takehiro.c
    src/util.c
    src/vbrquantize.c
    src/version.c
)

target_include_directories(mp3lame
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(mp3lame PRIVATE HAVE_CONFIG_H PIC)

if(MSVC)
    target_include_directories(mp3lame PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/config/win)
    target_compile_definitions(mp3lame PRIVATE TAKEHIRO_IEEE754_HACK FLOAT8=float REAL_IS_FLOAT=1 BS_FORMAT=BINARY)
else()
    target_include_directories(mp3lame PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/config/mac)
endif()

# Los .c de LAME disparan warnings estrictos del proyecto; se compilan con warnings
# base para no ensuciar el build de la app.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(mp3lame PRIVATE -w)
elseif(MSVC)
    target_compile_options(mp3lame PRIVATE /w)
endif()
