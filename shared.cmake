# Loaded by wxl-core v1.1.247 when this repository is copied to
# extensions/wxl-controller-input. Keep SDL exact and static.
include(FetchContent)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_AUDIO OFF CACHE BOOL "" FORCE)
set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
set(SDL_DIALOG OFF CACHE BOOL "" FORCE)
set(SDL_GPU OFF CACHE BOOL "" FORCE)
set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
set(SDL_POWER OFF CACHE BOOL "" FORCE)
set(SDL_RENDER OFF CACHE BOOL "" FORCE)
set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
set(SDL_TRAY OFF CACHE BOOL "" FORCE)
set(SDL_VIDEO OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG 8e37db5e797b6167f3a00d697d816a684bd259c7)
FetchContent_MakeAvailable(SDL3)
# Freeze the extension-local path now: arguments to a deferred call are otherwise
# evaluated later in the parent CMakeLists.txt directory.
cmake_language(EVAL CODE
    "cmake_language(DEFER CALL target_include_directories wxl-controller-input PRIVATE
        [[${CMAKE_CURRENT_LIST_DIR}/src]])")
cmake_language(DEFER CALL target_link_libraries wxl-controller-input PRIVATE SDL3::SDL3-static)
