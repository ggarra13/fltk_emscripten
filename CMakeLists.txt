cmake_minimum_required(VERSION 3.20)
project(fltk_emscripten)

set(CMAKE_CXX_STANDARD 17)
set(FLTK_BUILD_TEST OFF CACHE BOOL " " FORCE)
set(FLTK_USE_PTHREADS OFF CACHE BOOL " " FORCE)
set(FLTK_BUILD_FLUID OFF CACHE BOOL " " FORCE)
set(FLTK_BUILD_FLTK_OPTIONS OFF CACHE BOOL " " FORCE)
set(FLTK_BUILD_GL ON CACHE BOOL " " FORCE)
#set(FLTK_BUILD_WEBGPU ON CACHE BOOL " " FORCE)
set(FLTK_BUILD_PDF_DOCS OFF CACHE BOOL " " FORCE)
set(FLTK_OPTION_PEN_SUPPORT OFF CACHE BOOL " " FORCE)

include(FetchContent)
FetchContent_Declare(
    FLTK
    GIT_REPOSITORY      git@github.com:ggarra13/fltk.git
    GIT_TAG             emscripten
    #GIT_TAG             vk
    GIT_SHALLOW         ON
)

FetchContent_MakeAvailable(FLTK)

# --- DEBUG & READABILITY FLAGS ---
# Use -g3 for readable JS glue code with real variable/function names.
# Use -gsource-map instead if you want full C++ Source Maps (lets you debug .cxx files directly in browser DevTools).
set(EM_DEBUG_FLAGS "-O0 -g3 -gsource-map")
set(EM_RELEASE_FLAGS "-Wl,--strip-all")


set(FLTK_GL)
set(GL_LINK_FLAGS "")
if(FLTK_BUILD_GL)
    set(FLTK_GL fltk::gl)
    set(GL_LINK_FLAGS "-sMAX_WEBGL_VERSION=2 -sMIN_WEBGL_VERSION=1 -sLEGACY_GL_EMULATION=1 -sGL_UNSAFE_OPTS=1")
    set(GL_TESTS src/unittest_gl.cxx)
endif()
set(WEBGPU_LINK_FLAGS "")
if(FLTK_BUILD_WEBGPU)
    set(WEBGPU_LINK_FLAGS "")
    set(WEBGPU_TESTS )
endif()


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
    src/unittest_text.cxx
    ${GL_TESTS}
    ${WEBGPU_TESTS}
)
target_link_libraries(unittests PRIVATE fltk::fltk fltk::images ${FLTK_GL})
set_target_properties(unittests PROPERTIES SUFFIX .html
    LINK_FLAGS "-s WASM=1 -sALLOW_MEMORY_GROWTH -sASYNCIFY -sMINIFY_HTML=0 -sOFFSCREENCANVAS_SUPPORT=1 ${GL_LINK_FLAGS} ${WEBGPU_LINK_FLAGS} --bind --shell-file ${CMAKE_CURRENT_LIST_DIR}/shell_minimal.html ${EM_DEBUG_FLAGS}")
