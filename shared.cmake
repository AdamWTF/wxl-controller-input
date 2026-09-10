# Loaded by wxl-core v1.1.247 when this repository is copied to
# extensions/wxl-controller-input. Keep SDL exact and static.
include(FetchContent)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG 8e37db5e797b6167f3a00d697d816a684bd259c7)
FetchContent_MakeAvailable(SDL3)
cmake_language(DEFER CALL target_link_libraries wxl-controller-input PRIVATE SDL3::SDL3-static)

