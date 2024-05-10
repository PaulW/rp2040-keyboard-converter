# Define list of common source files within project root
file(GLOB SRC_ROOT CMAKE_CONFIGURE_DEPENDS
  ${CMAKE_SOURCE_DIR}/*.c
)

# Define list of source files for common libraries
file(GLOB_RECURSE SRC_COMMON CONFIGURE_DEPENDS
  ${CMAKE_SOURCE_DIR}/common/*.c
)

# Define list of Common PIO files for compilation
file(GLOB_RECURSE COMMON_PIO_FILES CONFIGURE_DEPENDS
  ${CMAKE_SOURCE_DIR}/common/*.pio
)

list(APPEND PIO_FILES ${COMMON_PIO_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC
  ${CMAKE_SOURCE_DIR}/
  ${CMAKE_SOURCE_DIR}/common/lib/
)

target_sources(${PROJECT_NAME} PUBLIC
  ${SRC_ROOT}
  ${SRC_COMMON}
)