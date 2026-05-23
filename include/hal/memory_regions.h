/**
 * @file memory_regions.h
 * @brief Memory region enumeration API for HAL
 *
 * This module provides iterators to enumerate memory regions from the device tree:
 *
 * - **Memory Regions**: Physical memory regions available for use (from /memory nodes)
 * - **Reserved Regions**: Memory ranges reserved by the system and must not be allocated
 *   (from /reserved-memory node and memory reservations)
 *
 * ## Usage Pattern
 *
 * Both iterators follow the same init-then-iterate pattern:
 *
 * ```c
 * // Initialize iterator
 * hal_memory_iterator_t iter;
 * if (hal_get_memory_regions_iterator(&iter) != ERR_NONE)
 *     return; // Handle error
 *
 * // Iterate through regions
 * physical_memory_region_t region;
 * while (hal_get_next_memory_region(&iter, &region) == ERR_NONE) {
 *     // Process region
 * }
 * // Loop exits when hal_get_next_memory_region returns ERR_NOT_FOUND (end of iteration)
 * ```
 *
 * ## Return Value Semantics
 *
 * - `ERR_NONE`: Successfully retrieved next region; output parameter is valid
 * - `ERR_NOT_FOUND`: End of iteration reached (normal termination, not an error)
 * - Other error codes: Actual error occurred; output parameters remain unchanged
 */

#ifndef HAL_MEMORY_REGIONS
#define HAL_MEMORY_REGIONS

#include <stdbigos/error.h>
#include <stdbigos/memory_types.h>

/**
 * This type is opaque to the caller.
 * Fields of this struct should not be modified directly.
 * */
typedef struct {
	alignas(16) u8 data[32];
} hal_reserved_memory_iterator_t;

/**
 * Initialize an iterator for enumerating system-reserved memory regions.
 *
 * Reserved regions come from two sources in the device tree:
 * - The /reserved-memory node and its children
 * - The memory reservation table (memreserve entries)
 *
 * @param iterOUT Pointer to uninitialized iterator to fill.
 *
 * @retval ERR_NONE Iterator initialized successfully
 * @retval ERR_BAD_ARG Invalid argument (null parameter or device tree validation error)
 * @retval ERR_NOT_INITIALIZED HAL is not initialized
 * @retval Other error codes Device tree parsing failures
 *
 * @note Upon error, @p iterOUT should remain unchanged.
 */
error_t hal_get_reserved_regions_iterator(hal_reserved_memory_iterator_t* iterOUT);

/**
 * Retrieve the next reserved memory region.
 *
 * Call this repeatedly after hal_get_reserved_regions_iterator() until it returns ERR_NOT_FOUND.
 *
 * @param iter Pointer to active iterator (initialized by hal_get_reserved_regions_iterator())
 * @param areaOUT Pointer to output structure. On success, filled with region details.
 *
 * @retval ERR_NONE Region retrieved successfully
 * @retval ERR_BAD_ARG Invalid argument (null parameter or device tree validation error)
 * @retval ERR_NOT_FOUND No more regions (end of iteration)
 * @retval Other error codes Device tree errors
 *
 * @note Upon error, @p areaOUT should remain unchanged.
 */
error_t hal_get_next_reserved_region(hal_reserved_memory_iterator_t* iter, memory_area_t* areaOUT);

/**
 * This type is opaque to the caller.
 * Fields of this struct should not be modified directly.
 * */
typedef struct {
	alignas(16) u8 data[32];
} hal_memory_iterator_t;

/**
 * Initialize an iterator for enumerating physical memory regions.
 *
 * Enumerates all /memory nodes from the device tree, including all register entries
 * within each memory node.
 *
 * @param iterOUT Pointer to uninitialized iterator to fill.
 *
 * @retval ERR_NONE Iterator initialized successfully
 * @retval ERR_BAD_ARG Invalid argument (null parameter or device tree validation error)
 * @retval ERR_NOT_INITIALIZED HAL is not initialized
 * @retval Other error codes Device tree parsing failures
 *
 * @note Upon error, @p iterOUT should remain unchanged.
 */
error_t hal_get_memory_regions_iterator(hal_memory_iterator_t* iterOUT);

/**
 * Retrieve the next available memory region.
 *
 * Call this repeatedly after hal_get_memory_regions_iterator() until it returns ERR_NOT_FOUND.
 * Each call retrieves one register entry from the device tree memory nodes.
 *
 * @param iter Pointer to active iterator (initialized by hal_get_memory_regions_iterator())
 * @param areaOUT Pointer to output structure. On success, filled with region details.
 *
 * @retval ERR_NONE Region retrieved successfully
 * @retval ERR_BAD_ARG Invalid argument (null parameter or device tree validation error)
 * @retval ERR_NOT_FOUND No more regions (end of iteration)
 * @retval Other error codes Device tree errors
 *
 * @note Upon error, @p areaOUT should remain unchanged.
 */
error_t hal_get_next_memory_region(hal_memory_iterator_t* iter, physical_memory_region_t* areaOUT);

#endif // !HAL_MEMORY_REGIONS
