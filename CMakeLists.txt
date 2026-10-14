cmake_minimum_required(VERSION 3.20)
project(fltk_emscripten)

set(FLTK_BUILD_TEST OFF CACHE BOOL " " FORCE)
set(FLTK_USE_PTHREADS OFF CACHE BOOL " " FORCE)
set(FLTK_BUILD_FLUID OFF CACHE BOOL " " FORCE)
set(FLTK_BUILD_FLTK_OPTIONS OFF CACHE BOOL " " FORCE)
set(FLTK_BUILD_GL ON CACHE BOOL " " FORCE)

include(FetchContent)
FetchContent_Declare(
    FLTK
    GIT_REPOSITORY      https://github.com/MoAlyousef/fltk_wasm32_emscripten
    GIT_TAG             emscripten
    GIT_SHALLOW         ON
)

FetchContent_MakeAvailable(FLTK)

add_executable(index src/index.cpp)
target_link_libraries(index PRIVATE fltk::fltk fltk::images)
set_target_properties(index PROPERTIES SUFFIX .html LINK_FLAGS "-s WASM=1 -sALLOW_MEMORY_GROWTH -sASYNCIFY -sOFFSCREENCANVAS_SUPPORT=1 --bind --preload-file ${CMAKE_CURRENT_LIST_DIR}/images@. -sMINIFY_HTML=0 --shell-file ${CMAKE_CURRENT_LIST_DIR}/shell_minimal.html -Wl,--strip-all")

add_executable(simple src/simple.cpp)
target_link_libraries(simple PRIVATE fltk::fltk fltk::images)
set_target_properties(simple PROPERTIES SUFFIX .html LINK_FLAGS "-s WASM=1 -sALLOW_MEMORY_GROWTH -sASYNCIFY -sMINIFY_HTML=0 -sOFFSCREENCANVAS_SUPPORT=1 --bind --preload-file ${CMAKE_CURRENT_LIST_DIR}/images@. --shell-file ${CMAKE_CURRENT_LIST_DIR}/shell_minimal.html -Wl,--strip-all")

add_executable(unittests 
    src/unittest_about.cxx           
    src/unittest_fast_shapes.cxx  
    src/unittest_scrollbarsize.cxx  
    src/unittest_unicode.cxx
    src/unittest_gl.cxx
    src/unittest_circles.cxx         
    src/unittest_images.cxx       
    src/unittest_symbol.cxx         
    src/unittest_viewport.cxx
    src/unittest_complex_shapes.cxx  
    src/unittest_points.cxx       
    src/unittest_terminal.cxx       
    src/unittests.cxx
    src/unittest_core.cxx            
    src/unittest_schemes.cxx      
    src/unittest_text.cxx)
target_link_libraries(unittests PRIVATE fltk::fltk fltk::images GL)
set_target_properties(unittests PROPERTIES SUFFIX .html LINK_FLAGS "-s WASM=1 -sALLOW_MEMORY_GROWTH -sASYNCIFY -sMINIFY_HTML=0 -sOFFSCREENCANVAS_SUPPORT=1 --bind --shell-file ${CMAKE_CURRENT_LIST_DIR}/shell_minimal.html -Wl,--strip-all")
