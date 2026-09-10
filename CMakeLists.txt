cmake_minimum_required(VERSION 3.25)
project(wxl-controller-input VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(WXL_CONTROLLER_BUILD_TESTS "Build deterministic policy tests" ON)
option(WXL_CONTROLLER_BUILD_EXTENSION "Build the WarcraftXL extension" ${WIN32})

add_library(wxl-controller-policy STATIC
    src/bindings/Bindings.cpp
    src/bridge/BindingCapture.cpp
    src/camera/CameraController.cpp
    src/config/Config.cpp
    src/controller/ControllerSelector.cpp
    src/input/Deadzone.cpp
    src/input/ModifierController.cpp
    src/movement/MovementController.cpp
    src/persistence/AtomicFile.cpp
    src/persistence/BindingStore.cpp
    src/persistence/Json.cpp
    src/profiles/ProfileResolver.cpp)
target_include_directories(wxl-controller-policy PUBLIC src)
if(MSVC)
    target_compile_options(wxl-controller-policy PRIVATE /W4 /WX)
endif()

if(WXL_CONTROLLER_BUILD_TESTS)
    enable_testing()
    add_executable(wxl-controller-tests tests/PolicyTests.cpp)
    target_link_libraries(wxl-controller-tests PRIVATE wxl-controller-policy)
    if(MSVC)
        target_compile_options(wxl-controller-tests PRIVATE /W4 /WX)
    endif()
    add_test(NAME controller-policy COMMAND wxl-controller-tests)
endif()

if(WXL_CONTROLLER_BUILD_EXTENSION)
    if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(FATAL_ERROR "wxl-controller-input.dll must be configured with a Win32 toolchain")
    endif()
    if(NOT WXL_CORE_ROOT)
        message(FATAL_ERROR "Set WXL_CORE_ROOT to the pinned wxl-core v1.1.247 checkout")
    endif()

    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    # The extension only uses SDL's gamepad/joystick and event APIs. Keeping
    # unrelated subsystems out reduces both the DLL's attack surface and the
    # cost of producing the required statically linked Win32 artifact.
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

    add_library(wxl-controller-input SHARED
        src/Module.cpp
        src/ExtensionApi.cpp
        src/bridge/ControllerBridge.cpp
        src/bridge/LuaBridge.cpp
        src/controller/SdlControllerBackend.cpp
        src/diagnostics/DebugPanel.cpp
        src/game/NativeGameAdapter.cpp
        src/runtime/FeatureController.cpp)
    target_link_libraries(wxl-controller-input PRIVATE wxl-controller-policy SDL3::SDL3-static)
    target_include_directories(wxl-controller-input PRIVATE
        src
        "${WXL_CORE_ROOT}/include"
        "${WXL_CORE_ROOT}/src")
    target_compile_definitions(wxl-controller-input PRIVATE
        WXL_EXTENSION WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
    if(MSVC)
        target_compile_options(wxl-controller-input PRIVATE /W4 /WX)
    endif()
    set_target_properties(wxl-controller-input PROPERTIES PREFIX "" OUTPUT_NAME "wxl-controller-input")
endif()
