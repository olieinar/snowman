# C:/code/snowman/lib/CMakeLists.txt

# 1) Source files
set(SNOWMAN_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/agc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/audio-lib.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dtw-lib.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/eavesdrop-stream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/feat-lib.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fft-stream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/framer-stream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/frontend-lib.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/frontend-stream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gain-control-stream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/intercept-stream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/license-lib.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/matrix-wrapper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mfcc-stream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/nnet-component.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/nnet-lib.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/nnet-stream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/noise_suppression_x.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ns3.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pipeline-detect.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pipeline-lib.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pipeline-personal-enroll.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pipeline-template-cut.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pipeline-vad.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/raw-energy-vad-stream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/raw-nnet-vad-stream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/snowboy-debug.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/snowboy-detect-c.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/snowboy-detect.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/snowboy-error.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/snowboy-io.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/snowboy-math.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/snowboy-options.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/snowboy-utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/char_to_string.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/srfft.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tdereverb_x.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/template-container.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/template-detect-stream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/template-enroll-stream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/universal-detect-stream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vad-lib.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vad-state-stream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vector-wrapper.cpp
)

# 2) Options
option(SNOWMAN_BUILD_SHARED "Build snowman as a shared library" OFF)
option(SNOWMAN_BUILD_WITH_SSE3 "Enable SSE3" OFF)
option(SNOWMAN_BUILD_WITH_SSE4 "Enable SSE4.2" OFF)
option(SNOWMAN_BUILD_WITH_AVX  "Enable AVX" OFF)
option(SNOWMAN_BUILD_WITH_AVX2 "Enable AVX2" OFF)
option(SNOWMAN_BUILD_NATIVE    "Enable -march=native (non-MSVC)" OFF)

# 3) Library target
if(SNOWMAN_BUILD_SHARED)
  add_library(snowman SHARED ${SNOWMAN_SRC})
else()
  add_library(snowman STATIC ${SNOWMAN_SRC})
endif()

target_compile_features(snowman PRIVATE cxx_std_11)
target_include_directories(snowman PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# 4) Warnings per compiler
if(MSVC)
  target_compile_options(snowman PRIVATE /W4)
else()
  target_compile_options(snowman PRIVATE -Wall -Wextra -Winit-self -fno-omit-frame-pointer)
endif()

# 5) BLAS/LAPACK + threads
find_package(Threads REQUIRED)

if(MSVC)
  # Use OpenBLAS via vcpkg:  vcpkg install openblas:x64-windows
  find_package(OpenBLAS CONFIG REQUIRED)
  target_link_libraries(snowman PRIVATE OpenBLAS::OpenBLAS Threads::Threads)
else()
  # Prefer packaged BLAS/LAPACK if available; fall back to ATLAS names
  find_package(BLAS)
  find_package(LAPACK)
  if(BLAS_FOUND OR LAPACK_FOUND)
    target_link_libraries(snowman PRIVATE ${BLAS_LIBRARIES} ${LAPACK_LIBRARIES} Threads::Threads m)
  else()
    target_link_directories(snowman PRIVATE /usr/lib/atlas-base)
    target_link_libraries(snowman PRIVATE Threads::Threads m f77blas cblas lapack_atlas atlas)
  endif()
endif()

# 6) SIMD flags
if(MSVC)
  if(SNOWMAN_BUILD_WITH_AVX)
    target_compile_options(snowman PRIVATE /arch:AVX)
  endif()
  if(SNOWMAN_BUILD_WITH_AVX2)
    target_compile_options(snowman PRIVATE /arch:AVX2)
  endif()
else()
  if(SNOWMAN_BUILD_WITH_SSE3)
    target_compile_options(snowman PRIVATE -msse -msse2 -msse3 -mssse3)
  endif()
  if(SNOWMAN_BUILD_WITH_SSE4)
    target_compile_options(snowman PRIVATE -msse4.2)
  endif()
  if(SNOWMAN_BUILD_WITH_AVX)
    target_compile_options(snowman PRIVATE -mavx)
  endif()
  if(SNOWMAN_BUILD_WITH_AVX2)
    target_compile_options(snowman PRIVATE -mavx2)
  endif()
  if(SNOWMAN_BUILD_NATIVE)
    target_compile_options(snowman PRIVATE -march=native -mtune=native)
  endif()
endif()

# Alias to keep original target name expectations
add_library(snowboy ALIAS snowman)
