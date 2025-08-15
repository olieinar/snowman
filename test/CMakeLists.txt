# test/CMakeLists.txt

enable_testing()
include(FetchGoogleTest)
include(GoogleTest)

add_executable(snowboy-test
  helper.cpp
  inspector.cpp
  ClassifyTest.cpp
  EnrollTest.cpp
  DtwTest.cpp
  CutTest.cpp
  VectorTest.cpp
)

target_include_directories(snowboy-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Link snowman + gtest; use BCrypt on Windows, OpenSSL::Crypto elsewhere if available
target_link_libraries(snowboy-test PRIVATE snowman gtest gtest_main)
if (WIN32)
  target_link_libraries(snowboy-test PRIVATE bcrypt)
else()
  find_package(OpenSSL QUIET)
  if (OpenSSL_FOUND)
    target_link_libraries(snowboy-test PRIVATE OpenSSL::Crypto)
  endif()
endif()

gtest_discover_tests(snowboy-test)

if (LTOAvailable)
  set_property(TARGET snowboy-test PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()
