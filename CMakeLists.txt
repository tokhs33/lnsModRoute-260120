# This file is just an orchestration
cmake_minimum_required(VERSION 3.18)

option(CMAKE_EXPORT_COMPILE_COMMANDS "Export compile command" OFF)

project(lnsmodroute VERSION 0.9.7 LANGUAGES CXX)
message(STATUS "${PROJECT_NAME} version: ${PROJECT_VERSION}")

set(CMAKE_CXX_STANDARD 20)
if(MSVC)
  add_compile_definitions(_USE_MATH_DEFINES)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
option(ENABLE_PYTHON, "Enable Python binding" ON)
option(ENABLE_JNI, "Enable JNI binding" ON)
option(CODE_COVERAGE, "Enable coverage reporting" OFF)

# Add -fPIC option for all targets
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.2
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF) # Disable Google Test installation
FetchContent_MakeAvailable(googletest)

# Default Build Type to be Release
get_property(isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(isMultiConfig)
  if(NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Release;Debug" CACHE STRING
    "Choose the type of builds, options are: Debug Release RelWithDebInfo MinSizeRel. (default: Release;Debug)"
    FORCE)
  endif()
  message(STATUS "Configuration types: ${CMAKE_CONFIGURATION_TYPES}")
else()
  if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING
    "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel. (default: Release)"
    FORCE)
  endif()
  message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
endif()

# Layout build dir like install dir
include(GNUInstallDirs)
if(UNIX)
  option(BUILD_SHARED_LIBS "Build shared libraries (.so or .dylib)." ON)
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
  # for multi-config build system (e.g. Xcode, Ninja Multi-Config)
  foreach(OutputConfig IN LISTS CMAKE_CONFIGURATION_TYPES)
    string(TOUPPER ${OutputConfig} OUTPUTCONFIG)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/${OutputConfig}/${CMAKE_INSTALL_LIBDIR})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/${OutputConfig}/${CMAKE_INSTALL_LIBDIR})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/${OutputConfig}/${CMAKE_INSTALL_BINDIR})
  endforeach()
else()
  # Currently Only support static build for windows
  option(BUILD_SHARED_LIBS "Build shared libraries (.dll)." OFF)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
  # for multi-config builds (e.g. msvc)
  foreach(OutputConfig IN LISTS CMAKE_CONFIGURATION_TYPES)
    string(TOUPPER ${OutputConfig} OUTPUTCONFIG)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/${OutputConfig}/${CMAKE_INSTALL_BINDIR})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/${OutputConfig}/${CMAKE_INSTALL_BINDIR})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/${OutputConfig}/${CMAKE_INSTALL_BINDIR})
  endforeach()
endif()

find_package(Threads REQUIRED)
link_libraries(${CMAKE_THREAD_LIBS_INIT})

# 라이브러리 찾기
# find_package(lnspdptw REQUIRED)  # find_package()는 CMAKE용 패키지 설정이 없어서 실패
if(LNSPDPTW_PATH)
  set(lnspdptw_INCLUDE_DIRS ${LNSPDPTW_PATH}/include)
  if(WIN32)
    set(lnspdptw_LIBRARIES ${LNSPDPTW_PATH}/lib/lnspdptw_static.lib)
  else()
    set(lnspdptw_LIBRARIES ${LNSPDPTW_PATH}/lib/liblnspdptw_static.a)
  endif()
else()
  set(lnspdptw_INCLUDE_DIRS ${CMAKE_INSTALL_PREFIX}/include)
  if(WIN32)
    set(lnspdptw_LIBRARIES ${CMAKE_INSTALL_PREFIX}/lib/lnspdptw_static.lib)
  else()
    set(lnspdptw_LIBRARIES ${CMAKE_INSTALL_PREFIX}/lib/liblnspdptw_static.a)
  endif()
endif()

if(NOT lnspdptw_INCLUDE_DIRS OR NOT lnspdptw_LIBRARIES)
  message(FATAL_ERROR "lnspdptw path not set or libraries not found")
endif()

set(MOD_BASIC_SOURCES
  src/lnsModRoute.cc
  src/costCache.cc
  src/queryOsrmCost.cc
  src/queryValhallaCost.cc
  src/threadPool.cc
  src/requestLogger.cc
  include/gason/gason.cpp)
add_executable(${PROJECT_NAME}
  src/main.cc
  src/main_utility.cc
  ${MOD_BASIC_SOURCES}
)
# 라이브러리의 헤더 파일 포함
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${lnspdptw_INCLUDE_DIRS}
)
# target_compile_definitions(${PROJECT_NAME} PRIVATE -DCPPHTTPLIB_OPENSSL_SUPPORT)
target_link_libraries(${PROJECT_NAME} PRIVATE ${lnspdptw_LIBRARIES})

add_executable(test_modroute
  src/test_modroute.cc
  src/main_utility.cc
  src/lib_modroute.cc
  ${MOD_BASIC_SOURCES}
)
target_include_directories(test_modroute PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${lnspdptw_INCLUDE_DIRS}
)
target_link_libraries(test_modroute PRIVATE ${lnspdptw_LIBRARIES})

if(ENABLE_PYTHON)
  # pybind11 다운로드 설정
  FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG v2.13.6  # 사용하고자 하는 pybind11의 버전 지정 (예: v2.13.6)
  )
  FetchContent_MakeAvailable(pybind11)

  # pybind11을 사용하여 모듈 빌드
  add_library(mod_route MODULE src/py_modroute.cc src/lib_modroute.cc ${MOD_BASIC_SOURCES})
  target_include_directories(mod_route PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${lnspdptw_INCLUDE_DIRS}
  )
  target_link_libraries(mod_route PRIVATE pybind11::module ${lnspdptw_LIBRARIES})
  set_target_properties(mod_route PROPERTIES PREFIX "")
endif()

if(ENABLE_JNI)
  find_package(JNI REQUIRED)

  add_library(lib_jni SHARED src/jni_modroute.cc src/lib_modroute.cc ${MOD_BASIC_SOURCES})
  set_target_properties(lib_jni PROPERTIES OUTPUT_NAME modroute_jni)
  target_include_directories(lib_jni PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${lnspdptw_INCLUDE_DIRS}
    ${JNI_INCLUDE_DIRS}
  )
  target_link_libraries(lib_jni PRIVATE JNI::JNI ${lnspdptw_LIBRARIES})
endif()

find_program(LCOV_PATH lcov)
find_program(GENHTML_PATH genhtml)

# # activate CTest
# include(CTest)
# enable_testing()
# include(GoogleTest)

# # include CMakeLists.txt in test directory
# add_subdirectory(tests)

# Install rules
include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(FILES lnsmodroute.yaml DESTINATION bin)
