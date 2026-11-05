# Host tests (desktop C compiler: MSVC, GCC or Clang). The plugin itself is built with the Makefile (devkitARM).
#   cmake -S . -B build-tests && cmake --build build-tests && ctest --test-dir build-tests --output-on-failure
# Optional data (not distributed, see README.md):
#   OOT3D_CANONICAL_CODE   path to a code image you extracted from your own game (enables the signature/hook checks)
#   OCARINA_RECORDINGS_DIR folder with rec1.raw/rec2.raw microphone takes (enables the recording checks)
cmake_minimum_required(VERSION 3.16)
project(ocarina_3ds_tests C)
set(CMAKE_C_STANDARD 99)
enable_testing()

set(SRC   "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(RECOG "${SRC}/recog")
set(TESTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")

# detector and matcher library
add_executable(test_ocarina ${TESTS}/test_ocarina.c ${RECOG}/pitch.c ${RECOG}/notes.c)
target_include_directories(test_ocarina PRIVATE ${RECOG})
add_test(NAME ocarina_pitch_notes COMMAND test_ocarina)

add_executable(test_matcher ${TESTS}/test_matcher.c ${RECOG}/matcher.c ${RECOG}/catalog.c)
target_include_directories(test_matcher PRIVATE ${RECOG})
add_test(NAME ocarina_matcher COMMAND test_matcher)

# plugin logic (everything except the console glue in src/ocarina.c and src/start.s)
add_executable(test_hook ${TESTS}/test_hook.c ${SRC}/hook.c ${SRC}/scan.c)
target_include_directories(test_hook PRIVATE ${SRC})
add_test(NAME plugin_hook COMMAND test_hook)

add_executable(test_fslog ${TESTS}/test_fslog.c ${SRC}/fslog.c)
target_include_directories(test_fslog PRIVATE ${SRC})
add_test(NAME plugin_fslog COMMAND test_fslog)

add_executable(test_integ ${TESTS}/test_integ.c
               ${SRC}/integ.c ${SRC}/drv.c ${SRC}/dlg.c ${SRC}/dlgmem.c ${SRC}/dlglog.c ${SRC}/logbuf.c
               ${SRC}/inject.c ${SRC}/freerange.c ${SRC}/ringcopy.c
               ${RECOG}/pitch.c ${RECOG}/notes.c ${RECOG}/matcher.c ${RECOG}/catalog.c)
target_include_directories(test_integ PRIVATE ${SRC} ${RECOG})
add_test(NAME plugin_integ COMMAND test_integ)

if(NOT MSVC)
  target_link_libraries(test_ocarina m)
  target_link_libraries(test_integ m)
endif()
