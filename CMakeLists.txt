# CMakeList.txt : CMake project for smol, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

set (CMAKE_CXX_STANDARD 20)
set (CMAKE_C_STANDARD 99)

project ("smol")
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
# Add source to this project's executable.
add_executable (smol
	"microui-source.c"
	"atlas.c"
	"smol.cpp"
        "tree-sitter/lib/src/lib.c"
)
target_include_directories(smol  PUBLIC "sdl/include")
target_include_directories(smol  PUBLIC "microui")
target_include_directories(smol  PUBLIC "tree-sitter/lib/src")
target_include_directories(smol  PUBLIC "tree-sitter/lib/include")

link_directories(${CMAKE_SOURCE_DIR}/sdl/lib/x64/)

# target_link_libraries(smol opengl32)
target_link_libraries(smol OpenGL::GL)
target_link_libraries(smol SDL2::SDL2)
# target_link_libraries(smol ${CMAKE_SOURCE_DIR}/sdl/lib/x64/SDL2.lib)
# target_link_libraries(smol ${CMAKE_SOURCE_DIR}/sdl/lib/x64/SDL2.lib)
