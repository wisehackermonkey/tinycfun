cmake_minimum_required(VERSION 3.15)
project(tipdemo C)

# Compile the resource script containing the model
enable_language(RC)
set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR}")

add_library(tipdemo SHARED tipdemo.c chime.o model.rc)

# Ensure cifg_int8.bin is present (fail early if not)
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cifg_int8.bin")
    message(FATAL_ERROR "cifg_int8.bin must be in ${CMAKE_CURRENT_SOURCE_DIR} (referenced by model.rc)")
endif()

target_link_libraries(tipdemo ole32 oleaut32 uuid user32 gdi32 advapi32)
target_compile_definitions(tipdemo PRIVATE UNICODE _UNICODE)