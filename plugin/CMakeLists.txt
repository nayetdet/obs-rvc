cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
option(ENABLE_QT "Use Qt functionality" OFF)

include(compilerconfig)
include(defaults)
include(helpers)

include(FetchContent)

set(BUILD_CXX ON CACHE BOOL "Build iceoryx2 C++ bindings" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "Build iceoryx2 examples" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Build iceoryx2 tests" FORCE)

FetchContent_Declare(
  iceoryx2
  GIT_REPOSITORY https://github.com/eclipse-iceoryx/iceoryx2.git
  GIT_TAG v0.9.3
  GIT_SHALLOW TRUE
  EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(iceoryx2)

if(APPLE AND CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
  find_program(CARGO_EXECUTABLE cargo REQUIRED)
  find_program(RUSTUP_EXECUTABLE rustup REQUIRED)

  set(ICEORYX2_RUST_X86_TARGET_DIR "${CMAKE_BINARY_DIR}/rust/x86_64-apple-darwin")
  set(ICEORYX2_RUST_X86_ARTIFACT_DIR "${ICEORYX2_RUST_X86_TARGET_DIR}/x86_64-apple-darwin/release")
  set(ICEORYX2_RUST_NATIVE_ARTIFACT_DIR "${CMAKE_BINARY_DIR}/rust/native/release")

  add_custom_target(
    iceoryx2-ffi-c-universal
    COMMAND "${RUSTUP_EXECUTABLE}" target add x86_64-apple-darwin
    COMMAND
      "${CARGO_EXECUTABLE}" build --release --no-default-features --features=iceoryx2-ffi-c/std,iceoryx2-bb-loggers/std
      --package iceoryx2-ffi-c --target-dir "${ICEORYX2_RUST_X86_TARGET_DIR}" --target x86_64-apple-darwin
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${ICEORYX2_RUST_NATIVE_ARTIFACT_DIR}"
    COMMAND
      /usr/bin/lipo -create "${ICEORYX2_RUST_NATIVE_ARTIFACT_DIR}/libiceoryx2_ffi_c.a"
      "${ICEORYX2_RUST_X86_ARTIFACT_DIR}/libiceoryx2_ffi_c.a" -output
      "${ICEORYX2_RUST_NATIVE_ARTIFACT_DIR}/libiceoryx2_ffi_c.universal.a"
    COMMAND
      "${CMAKE_COMMAND}" -E copy "${ICEORYX2_RUST_NATIVE_ARTIFACT_DIR}/libiceoryx2_ffi_c.universal.a"
      "${ICEORYX2_RUST_NATIVE_ARTIFACT_DIR}/libiceoryx2_ffi_c.a"
    COMMAND
      /usr/bin/lipo -create "${ICEORYX2_RUST_NATIVE_ARTIFACT_DIR}/libiceoryx2_ffi_c.dylib"
      "${ICEORYX2_RUST_X86_ARTIFACT_DIR}/libiceoryx2_ffi_c.dylib" -output
      "${ICEORYX2_RUST_NATIVE_ARTIFACT_DIR}/libiceoryx2_ffi_c.universal.dylib"
    COMMAND
      "${CMAKE_COMMAND}" -E copy "${ICEORYX2_RUST_NATIVE_ARTIFACT_DIR}/libiceoryx2_ffi_c.universal.dylib"
      "${ICEORYX2_RUST_NATIVE_ARTIFACT_DIR}/libiceoryx2_ffi_c.dylib"
    DEPENDS iceoryx2-ffi-c-build-step
    WORKING_DIRECTORY "${iceoryx2_SOURCE_DIR}"
    VERBATIM
  )
endif()

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(APPLE)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE iceoryx2-cxx-object-lib iceoryx2-c::static-lib)
  if(TARGET iceoryx2-ffi-c-universal)
    add_dependencies(${CMAKE_PROJECT_NAME} iceoryx2-ffi-c-universal)
  endif()
else()
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE iceoryx2-cxx::static-lib-cxx)
endif()

if(MSVC)
  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE /wd4068)
endif()

if(UNIX AND NOT APPLE)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_DL_LIBS})
endif()

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/plugin-main.c
    src/rvc/ipc/rvc-ipc.cpp
    src/rvc/filter/rvc-filter.cpp
    src/rvc/ipc/settings/rvc-settings-client.cpp
    src/rvc/ipc/audio/rvc-audio-client.cpp
    src/rvc/worker/worker-process.cpp
)
set_property(TARGET ${CMAKE_PROJECT_NAME} PROPERTY C_STANDARD 11)
set_property(TARGET ${CMAKE_PROJECT_NAME} PROPERTY CXX_STANDARD 17)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

set(WORKER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../plugin-worker")
set(WORKER_BUNDLE_DIR "${CMAKE_CURRENT_BINARY_DIR}/worker-bundle")
set(WORKER_BUNDLE_STAMP "${WORKER_BUNDLE_DIR}/complete")
file(GLOB_RECURSE WORKER_SOURCES CONFIGURE_DEPENDS "${WORKER_DIR}/src/*.py")
find_program(UV_EXECUTABLE uv REQUIRED)
if(UNIX AND NOT APPLE)
  find_program(WORKER_PYTHON NAMES python3.12 python3 PATHS /usr/bin NO_DEFAULT_PATH REQUIRED)
  set(UV_PYTHON_ARGS --python "${WORKER_PYTHON}")
endif()

add_custom_command(
  OUTPUT "${WORKER_BUNDLE_STAMP}"
  COMMAND
    "${UV_EXECUTABLE}" run --locked --project "${WORKER_DIR}" ${UV_PYTHON_ARGS} --no-build-isolation-package fairseq
    --with "pyinstaller>=6.11,<7" python "${WORKER_DIR}/scripts/build_bundle.py" --output "${WORKER_BUNDLE_DIR}"
  DEPENDS
    "${WORKER_DIR}/pyproject.toml"
    "${WORKER_DIR}/uv.lock"
    "${WORKER_DIR}/scripts/build_bundle.py"
    ${WORKER_SOURCES}
  COMMAND "${CMAKE_COMMAND}" -E touch "${WORKER_BUNDLE_STAMP}"
  WORKING_DIRECTORY "${WORKER_DIR}"
  COMMENT "Bundle OBS RVC Python worker"
  VERBATIM
)
add_custom_target(obs-rvc-worker ALL DEPENDS "${WORKER_BUNDLE_STAMP}")
add_dependencies(${CMAKE_PROJECT_NAME} obs-rvc-worker)

if(WIN32)
  set(WORKER_DEST "${CMAKE_PROJECT_NAME}/data/worker")
elseif(APPLE)
  set(WORKER_DEST "${CMAKE_PROJECT_NAME}.plugin/Contents/Resources/worker")
else()
  set(WORKER_DEST "${CMAKE_INSTALL_DATAROOTDIR}/obs/obs-plugins/${CMAKE_PROJECT_NAME}/worker")
endif()

install(DIRECTORY "${WORKER_BUNDLE_DIR}/obs-rvc-worker/" DESTINATION "${WORKER_DEST}" USE_SOURCE_PERMISSIONS)

add_custom_command(
  TARGET ${CMAKE_PROJECT_NAME}
  POST_BUILD
  COMMAND
    "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/rundir/$<CONFIG>/${CMAKE_PROJECT_NAME}/worker"
  COMMAND
    "${CMAKE_COMMAND}" -E copy_directory "${WORKER_BUNDLE_DIR}/obs-rvc-worker"
    "${CMAKE_CURRENT_BINARY_DIR}/rundir/$<CONFIG>/${CMAKE_PROJECT_NAME}/worker"
  COMMENT "Copy OBS RVC Python worker"
  VERBATIM
)
