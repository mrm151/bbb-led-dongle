cmake_minimum_required(VERSION 3.20.0)
project(bbbled_test)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/7e17b15f1547bb8dd9c2fed91043b7af3437387f.zip
)
FetchContent_MakeAvailable(googletest)

enable_testing()

include_directories(${GTEST_INCLUDE_DIRS} ../src)

add_executable(
  b64test
  test_b64.c
  ../src/b64.c
)
target_link_libraries(
  b64test
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(b64test)

