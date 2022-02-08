# CMakeList.txt : CMake project for smol, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)
set (CMAKE_CXX_STANDARD 20)

project ("smol")
# find_package(SDL2)
# Add source to this project's executable.
add_executable (smol
	"microui/microui-source.c"
	"microui/renderer.c"
	"smol.cpp"
)
target_include_directories(smol  PUBLIC "sdl/include")
target_include_directories(smol  PUBLIC "microui")

link_directories(${CMAKE_SOURCE_DIR}/sdl/lib/x64/)

target_link_libraries(smol opengl32)
target_link_libraries(smol ${CMAKE_SOURCE_DIR}/sdl/lib/x64/SDL2.lib)
