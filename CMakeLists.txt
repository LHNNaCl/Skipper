cmake_minimum_required(VERSION 3.21)
project(Skipper VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MINGW)
  add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

# nlohmann/json 3.11.3 and doctest 2.4.11 are vendored under third_party.
# GitHub clones time out on this machine, so the headers are stored locally
# instead of being pulled with FetchContent at configure time.
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE ${CMAKE_SOURCE_DIR}/third_party)
add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)

add_library(doctest INTERFACE)
target_include_directories(doctest INTERFACE ${CMAKE_SOURCE_DIR}/third_party)
add_library(doctest::doctest ALIAS doctest)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()
# Qt 6.5 turns on AUTOMOC and AUTOUIC only. The tray icon is a Qt resource.
set(CMAKE_AUTORCC ON)

add_library(skipper_domain
  src/domain/window.cpp
  src/domain/output_name.cpp
  src/domain/chapter_picker.cpp
  src/domain/audio_match.cpp
  src/domain/black_frame.cpp
  src/domain/resolver.cpp
  src/domain/edit_check.cpp
  src/domain/keep_ranges.cpp
)
target_include_directories(skipper_domain PUBLIC ${CMAKE_SOURCE_DIR}/src)

add_library(skipper_application
  src/application/cancel.cpp
  src/application/session.cpp
)
target_link_libraries(skipper_application PUBLIC skipper_domain)
target_include_directories(skipper_application PUBLIC ${CMAKE_SOURCE_DIR}/src)

add_library(skipper_infrastructure
  src/infrastructure/process.cpp
  src/infrastructure/file_listing.cpp
  src/infrastructure/output_files.cpp
  src/infrastructure/ffmpeg_probe.cpp
  src/infrastructure/ffmpeg_fingerprint.cpp
  src/infrastructure/ffmpeg_visual.cpp
  src/infrastructure/ffmpeg_export.cpp
)
target_link_libraries(skipper_infrastructure PUBLIC nlohmann_json::nlohmann_json)
target_include_directories(skipper_infrastructure PUBLIC ${CMAKE_SOURCE_DIR}/src)
if(WIN32)
  target_link_libraries(skipper_infrastructure PUBLIC ws2_32)
endif()

add_executable(skipper
  src/main.cpp
  src/ui/time_format.cpp
  src/ui/controller.cpp
  src/ui/main_window.cpp
  icons/skipper.qrc
  icons/skipper.rc
)
target_link_libraries(skipper PRIVATE skipper_domain skipper_application skipper_infrastructure Qt6::Widgets)
set_target_properties(skipper PROPERTIES AUTOMOC ON WIN32_EXECUTABLE ON)

# Full FFmpeg build (with chromaprint) is copied next to skipper.exe.
set(SKIPPER_FFMPEG_DIR ${CMAKE_SOURCE_DIR}/third_party/ffmpeg-2026-11-09-git-16a345b022-full_build/bin)
foreach(tool IN ITEMS ffmpeg.exe ffprobe.exe)
  if(EXISTS ${SKIPPER_FFMPEG_DIR}/${tool})
    add_custom_command(TARGET skipper POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SKIPPER_FFMPEG_DIR}/${tool}
        $<TARGET_FILE_DIR:skipper>/${tool}
      COMMENT "Copy ${tool} next to skipper")
  endif()
endforeach()

add_executable(skipper_domain_tests tests/domain/domain_tests.cpp)
target_link_libraries(skipper_domain_tests PRIVATE skipper_domain doctest::doctest)

enable_testing()
add_test(NAME domain COMMAND skipper_domain_tests)
