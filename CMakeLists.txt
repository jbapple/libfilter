cmake_minimum_required(VERSION 3.5)

add_executable(bench bench.cpp)
add_executable(example example.c)
add_executable(example.exe example.cpp)
target_link_libraries(example.exe filter)
add_library(filter STATIC lib/util.c lib/memory.c)
target_link_libraries(filter m)

install(TARGETS filter DESTINATION lib)
install(DIRECTORY include/ DESTINATION include FILES_MATCHING REGEX ".*\.h(pp)?$")

set(BOTH_FLAGS "-W -Wall -Wextra -Wno-unused-variable -Wno-unused-function -march=native -mtune=native -O1 -ggdb3")
# -fsanitize=address")
set(CMAKE_CXX_FLAGS "${BOTH_FLAGS} --std=gnu++17 -Wno-c++98-compat")
set(CMAKE_C_FLAGS "${BOTH_FLAGS} -Wno-missing-prototypes --std=gnu17")
target_link_libraries(example m filter)
# target_link_libraries(example m)
target_link_libraries(bench filter)
target_include_directories(example PUBLIC "include")
target_include_directories(filter PUBLIC "include")
target_include_directories(example PUBLIC "pcg-c/include")
target_include_directories(filter PUBLIC "pcg-c/include")

# include(ExternalProject)
# ExternalProject_Add(pcg-c PREFIX "${CMAKE_SOURCE_DIR}/pcg-c" SOURCE_DIR "${CMAKE_SOURCE_DIR}/pcg-c" CONFIGURE_COMMAND "" CMAKE_COMMAND "" BUILD_IN_SOURCE 1 BUILD_COMMAND "make" BUILD_ALWAYS INSTALL_COMMAND "")
# ExternalProject_Add_Step(pcg-c realinstall COMMAND "make install" DEPENDEES build WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/pcg-c")
# ExternalProject_Add_StepTargets(pcg-c realinstall)
# add_dependencies(install pcg-c-realinstall)

enable_testing()
# add_executable(basic test/basic.c)
# target_link_libraries(basic filter)
# add_test(basic basic)

add_executable(plot-optimal-split plot-optimal-split.cpp)
target_link_libraries(plot-optimal-split filter)

add_custom_command(OUTPUT plot-optimal-split.csv
  DEPENDS plot-optimal-split
  COMMAND plot-optimal-split > plot-optimal-split.csv)
add_custom_target(csv
  DEPENDS ${PROJECT_BINARY_DIR}/plot-optimal-split.csv)
add_custom_command(OUTPUT plot-optimal-split.pdf
  DEPENDS plot-optimal-split.gnuplot plot-optimal-split.csv
  COMMAND gnuplot5 ${PROJECT_SOURCE_DIR}/plot-optimal-split.gnuplot)
add_custom_target(pdf
  DEPENDS ${PROJECT_BINARY_DIR}/plot-optimal-split.pdf)
add_dependencies(pdf csv)

# Two source files, src1 and src2. src1 produces intermediate1 and
# intermediate1 and src2 produce output1.
#
# When I call make output1, if src2 has changed, src2 and
# intermediate1 should be used to create output1.
#
# When I call make output1, if src1 has changed or intermediate1 is
# absent, first src1 should be used to regenerate intermediate1 and
# then src2 and intermediate1 should be used to create output1.

# TODO: this is a fine hack for preventing make -j from running all
# benchmarks in parallel, but it makes making a single targeted
# outcome very slow, since it depends on some linear chain of other
# CSV benchmarks.
foreach(NDV 000000 00000 0000 000 00 0)
  foreach(BYTES 50 100 150 200 250)
    add_custom_command(OUTPUT bench-${BYTES}B-100${NDV}.csv
      DEPENDS bench
      COMMAND echo "start ${BYTES}B 100${NDV}"
      COMMAND bench --bytes ${BYTES}${NDV} --ndv 100${NDV} --reps 30 --print_header > bench-${BYTES}B-100${NDV}.csv
      COMMAND echo "end ${BYTES}B 100${NDV}")
    add_custom_target(csv-${BYTES}B-100${NDV}
      DEPENDS ${PROJECT_BINARY_DIR}/bench-${BYTES}B-100${NDV}.csv)
    if(${BYTES} STREQUAL 250)
      if (NOT ${NDV} STREQUAL 000000)
        add_dependencies(csv-${BYTES}B-100${NDV} csv-50B-1000${NDV})
      endif()
    else()
      math(EXPR NEW_BYTES "${BYTES} + 50")
      add_dependencies(csv-${BYTES}B-100${NDV} csv-${NEW_BYTES}B-100${NDV})
    endif()
  endforeach(BYTES)
endforeach(NDV)
