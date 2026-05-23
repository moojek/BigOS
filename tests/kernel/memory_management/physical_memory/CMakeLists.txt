add_executable(test_pmallocator
    test_allocator.c
    ${PROJECT_SOURCE_DIR}/src/kernel/memory_management/physical_memory/allocator.c
)

target_compile_features(test_pmallocator PUBLIC c_std_23)
target_compile_options(test_pmallocator PRIVATE -Wall -Wextra)

target_include_directories(test_pmallocator PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/external/include
    ${PROJECT_SOURCE_DIR}/src/kernel
    ${PROJECT_SOURCE_DIR}/src/kernel/memory_management/physical_memory
)

target_link_libraries(test_pmallocator PRIVATE unity)

add_test(NAME pmallocator_tests COMMAND test_pmallocator)

add_executable(test_hal_memory_regions
    test_hal_memory_regions.c
    ${PROJECT_SOURCE_DIR}/src/kernel/hal/arch/riscv/memory_region.c
    ${PROJECT_SOURCE_DIR}/src/lib/stdbigos/buffer.c
    ${PROJECT_SOURCE_DIR}/src/lib/stdbigos/bitutils.c
)

target_compile_features(test_hal_memory_regions PUBLIC c_std_23)
target_compile_options(test_hal_memory_regions PRIVATE -Wall -Wextra)

target_include_directories(test_hal_memory_regions PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/external/include
    ${PROJECT_SOURCE_DIR}/src/kernel
)

target_link_libraries(test_hal_memory_regions PRIVATE unity)

add_test(NAME hal_memory_regions_tests COMMAND test_hal_memory_regions)
