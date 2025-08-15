# apps/CMakeLists.txt

add_executable(cut
    helper.cpp
    cut.cpp
)
target_include_directories(cut PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(cut PRIVATE snowman)

add_executable(enroll
    helper.cpp
    enroll.cpp
)
target_include_directories(enroll PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
# On non-Windows we might use OpenSSL for MD5 in helper.cpp; on Windows it's BCrypt.
if (WIN32)
  target_link_libraries(enroll PRIVATE snowman bcrypt)
else()
  find_package(OpenSSL)
  if (OpenSSL_FOUND)
    target_link_libraries(enroll PRIVATE snowman OpenSSL::Crypto)
  else()
    target_link_libraries(enroll PRIVATE snowman)
  endif()
endif()

# --- Live apps ---
add_executable(detect-live
    helper.cpp
    detect-live.cpp
)
target_include_directories(detect-live PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if (WIN32)
  target_sources(detect-live PRIVATE pulseaudio_win.cpp)
  target_link_libraries(detect-live PRIVATE snowman bcrypt ole32 uuid)
  target_compile_definitions(detect-live PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0601)
else()
  target_link_libraries(detect-live PRIVATE snowman pulse-simple pulse)
endif()

add_executable(enroll-live
    helper.cpp
    enroll-live.cpp
)
target_include_directories(enroll-live PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if (WIN32)
  target_sources(enroll-live PRIVATE pulseaudio_win.cpp)
  target_link_libraries(enroll-live PRIVATE snowman bcrypt ole32 uuid)
  target_compile_definitions(enroll-live PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0601)
else()
  target_link_libraries(enroll-live PRIVATE snowman pulse-simple pulse)
endif()


if(LTOAvailable)
    message(STATUS "LTO enabled for apps")
    set_property(TARGET cut          PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    set_property(TARGET enroll       PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    set_property(TARGET detect-live  PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    set_property(TARGET enroll-live  PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()
