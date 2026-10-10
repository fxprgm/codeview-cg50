# CodeView: read Casio BASIC programs and Python scripts on the fx-CG50. Build: fxsdk build-cg
cmake_minimum_required(VERSION 3.15)
project(CodeView LANGUAGES C ASM)

include(GenerateG3A)
find_package(Gint 2.9 REQUIRED)

set(SOURCES
  src/main.c
  src/sources.c
  src/doc.c
  src/render.c
  src/list.c
  src/view.c
  src/fonts.c
  src/charmap.c
  src/sys.S
)

add_executable(codeview ${SOURCES})
target_compile_options(codeview PRIVATE -Wall -Wextra -O2 -g)
target_link_libraries(codeview Gint::Gint)

generate_g3a(TARGET codeview OUTPUT "CodeView.g3a"
  NAME "CodeView" ICONS assets-cg/icon-uns.png assets-cg/icon-sel.png)
