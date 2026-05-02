#include <dt/dt.h>
#include <hal/memory_regions.h>
#include <stdbigos/buffer.h>
#include <stdbigos/error.h>

#include "../../hal_internal.h"

static error_t hal_riscv_init_fdt(fdt_t* fdtOUT) {
	if (fdtOUT == nullptr)
		return ERR_BAD_ARG;

	if (!ihal_is_init())
		return ERR_NOT_INITIALIZED;

	void* dtb = nullptr;
	error_t err = ihal_get_dtb(&dtb);
	if (err)
		return err;

	return dt_init(dtb, fdtOUT);
}

static bool hal_riscv_node_name_is_memory(buffer_t node_name) {
	if (!buffer_is_valid(node_name))
		return false;

	const buffer_t memory_name = make_buffer("memory", 6);
	if (buffer_equal(node_name, memory_name))
		return true;

	if (node_name.size < 7)
		return false;

	const buffer_t memory_prefix = make_buffer("memory@", 7);
	const buffer_t node_prefix = buffer_sub_buffer(node_name, 0, 7);
	return buffer_equal(node_prefix, memory_prefix);
}

static error_t hal_riscv_node_is_memory(const fdt_t* fdt, dt_node_t node, bool* isMemoryOUT) {
	if (fdt == nullptr || isMemoryOUT == nullptr)
		return ERR_BAD_ARG;

	*isMemoryOUT = false;

	dt_prop_t device_type_prop;
	error_t err = dt_get_prop_by_name(fdt, node, "device_type", &device_type_prop);
	if (err == ERR_NONE) {
		buffer_t device_type_buf;
		err = dt_get_prop_buffer(fdt, device_type_prop, &device_type_buf);
		if (err)
			return err;

		if (buffer_equal(device_type_buf, make_buffer("memory", 6))) {
			*isMemoryOUT = true;
			return ERR_NONE;
		}
	} else if (err != ERR_NOT_FOUND) {
		return err;
	}

	buffer_t node_name;
	err = dt_get_node_name(fdt, node, &node_name);
	if (err)
		return err;

	*isMemoryOUT = hal_riscv_node_name_is_memory(node_name);
	return ERR_NONE;
}

static error_t hal_riscv_find_first_memory_node(const fdt_t* fdt, dt_node_t* nodeOUT) {
	if (fdt == nullptr || nodeOUT == nullptr)
		return ERR_BAD_ARG;

	dt_node_t node;
	error_t err = dt_get_node_child(fdt, fdt->root_node, &node);
	if (err)
		return err;

	while (true) {
		bool is_memory;
		err = hal_riscv_node_is_memory(fdt, node, &is_memory);
		if (err)
			return err;

		if (is_memory) {
			*nodeOUT = node;
			return ERR_NONE;
		}

		err = dt_get_node_sibling(fdt, node, &node);
		if (err)
			return err;
	}
}

static error_t hal_riscv_find_next_memory_node(const fdt_t* fdt, dt_node_t node, dt_node_t* nodeOUT) {
	if (fdt == nullptr || nodeOUT == nullptr)
		return ERR_BAD_ARG;

	error_t err = dt_get_node_sibling(fdt, node, &node);
	if (err)
		return err;

	while (true) {
		bool is_memory;
		err = hal_riscv_node_is_memory(fdt, node, &is_memory);
		if (err)
			return err;

		if (is_memory) {
			*nodeOUT = node;
			return ERR_NONE;
		}

		err = dt_get_node_sibling(fdt, node, &node);
		if (err)
			return err;
	}
}

static error_t hal_riscv_read_reg_entry(const fdt_t* fdt, dt_node_t node, u32 reg_idx, u64* addrOUT,
	                                       u64* sizeOUT) {
	if (fdt == nullptr || addrOUT == nullptr || sizeOUT == nullptr)
		return ERR_BAD_ARG;

	dt_prop_t reg_prop;
	error_t err = dt_get_prop_by_name(fdt, node, "reg", &reg_prop);
	if (err)
		return err;

	buffer_t reg_buf;
	err = dt_get_prop_buffer(fdt, reg_prop, &reg_buf);
	if (err)
		return err;

	const size_t entry_offset = (size_t)reg_idx * 2 * sizeof(u64);
	if (entry_offset >= reg_buf.size)
		return ERR_NOT_FOUND;
	if (reg_buf.size - entry_offset < 2 * sizeof(u64))
		return ERR_NOT_VALID;

	u64 addr;
	u64 size;
	if (!buffer_read_u64_be(reg_buf, entry_offset, &addr) || !buffer_read_u64_be(reg_buf, entry_offset + sizeof(u64), &size))
		return ERR_NOT_VALID;

	*addrOUT = addr;
	*sizeOUT = size;
	return ERR_NONE;
}

typedef struct {
	u32 idx;
	u32 reg_idx;
	bool is_in_resmem;
	dt_node_t node;
} riscv_hal_res_mem_iter_t;
static_assert(sizeof(riscv_hal_res_mem_iter_t) <= sizeof(hal_reserved_memory_iterator_t));

error_t hal_get_reserved_regions_iterator(hal_reserved_memory_iterator_t* iterOUT) {
	if (iterOUT == nullptr)
		return ERR_BAD_ARG;

	fdt_t fdt;
	error_t err = hal_riscv_init_fdt(&fdt);
	if (err)
		return err;

	const riscv_hal_res_mem_iter_t init = {
	    .idx = 0,
	    .reg_idx = 0,
	    .is_in_resmem = false,
	    .node = 0,
	};
	*(riscv_hal_res_mem_iter_t*)iterOUT = init;
	return ERR_NONE;
}

error_t hal_get_next_reserved_region(hal_reserved_memory_iterator_t* iter, memory_area_t* areaOUT) {
	if (iter == nullptr || areaOUT == nullptr)
		return ERR_BAD_ARG;

	fdt_t fdt;
	error_t err = hal_riscv_init_fdt(&fdt);
	if (err)
		return err;

	riscv_hal_res_mem_iter_t next_iter = *(riscv_hal_res_mem_iter_t*)iter;

	if (!next_iter.is_in_resmem) {
		fdt_rsv_entry entry;
		err = dt_get_rsv_mem_entry(&fdt, next_iter.idx, &entry);
		if (err == ERR_NONE) {
			memory_area_t area = {
			    .addr = (uintptr_t)entry.address,
			    .size = entry.size,
			};
			next_iter.idx += 1;
			*(riscv_hal_res_mem_iter_t*)iter = next_iter;
			*areaOUT = area;
			return ERR_NONE;
		}

		if (err != ERR_OUT_OF_BOUNDS)
			return err;

		dt_node_t reserved_mem_root;
		err = dt_get_node_by_path(&fdt, "/reserved-memory", &reserved_mem_root);
		if (err == ERR_NOT_FOUND)
			return ERR_NOT_FOUND;
		if (err)
			return err;

		err = dt_get_node_child(&fdt, reserved_mem_root, &next_iter.node);
		if (err == ERR_NOT_FOUND)
			return ERR_NOT_FOUND;
		if (err)
			return err;

		next_iter.reg_idx = 0;
		next_iter.is_in_resmem = true;
	}

	while (next_iter.node) {
		u64 addr;
		u64 size;
		err = hal_riscv_read_reg_entry(&fdt, next_iter.node, next_iter.reg_idx, &addr, &size);
		if (err == ERR_NONE) {
			memory_area_t area = {
			    .addr = (uintptr_t)addr,
			    .size = size,
			};
			next_iter.reg_idx += 1;
			*(riscv_hal_res_mem_iter_t*)iter = next_iter;
			*areaOUT = area;
			return ERR_NONE;
		}

		if (err != ERR_NOT_FOUND)
			return err;

		dt_node_t sibling;
		err = dt_get_node_sibling(&fdt, next_iter.node, &sibling);
		if (err == ERR_NONE) {
			next_iter.node = sibling;
			next_iter.reg_idx = 0;
			continue;
		}

		if (err == ERR_NOT_FOUND) {
			next_iter.node = 0;
			break;
		}

		return err;
	}

	*(riscv_hal_res_mem_iter_t*)iter = next_iter;
	return ERR_NOT_FOUND;
}

typedef struct {
	dt_node_t node;
	u32 reg_idx;
	bool has_node;
} riscv_hal_mem_iter_t;
static_assert(sizeof(riscv_hal_mem_iter_t) <= sizeof(hal_memory_iterator_t));

error_t hal_get_memory_regions_iterator(hal_memory_iterator_t* iterOUT) {
	if (iterOUT == nullptr)
		return ERR_BAD_ARG;

	fdt_t fdt;
	error_t err = hal_riscv_init_fdt(&fdt);
	if (err)
		return err;

	dt_node_t first_memory_node;
	err = hal_riscv_find_first_memory_node(&fdt, &first_memory_node);
	if (err)
		return err;

	const riscv_hal_mem_iter_t init = {
	    .node = first_memory_node,
	    .reg_idx = 0,
	    .has_node = true,
	};
	*(riscv_hal_mem_iter_t*)iterOUT = init;
	return ERR_NONE;
}

error_t hal_get_next_memory_region(hal_memory_iterator_t* iter, physical_memory_region_t* areaOUT) {
	if (iter == nullptr || areaOUT == nullptr)
		return ERR_BAD_ARG;

	riscv_hal_mem_iter_t next_iter = *(riscv_hal_mem_iter_t*)iter;
	if (!next_iter.has_node)
		return ERR_NOT_FOUND;

	fdt_t fdt;
	error_t err = hal_riscv_init_fdt(&fdt);
	if (err)
		return err;

	while (next_iter.has_node) {
		u64 addr;
		u64 size;
		err = hal_riscv_read_reg_entry(&fdt, next_iter.node, next_iter.reg_idx, &addr, &size);
		if (err == ERR_NONE) {
			const physical_memory_region_t area = {
			    .addr = (__phys void*)(uintptr_t)addr,
			    .size = size,
			};
			next_iter.reg_idx += 1;
			*(riscv_hal_mem_iter_t*)iter = next_iter;
			*areaOUT = area;
			return ERR_NONE;
		}

		if (err != ERR_NOT_FOUND)
			return err;

		dt_node_t next_memory_node;
		err = hal_riscv_find_next_memory_node(&fdt, next_iter.node, &next_memory_node);
		if (err == ERR_NONE) {
			next_iter.node = next_memory_node;
			next_iter.reg_idx = 0;
			next_iter.has_node = true;
			continue;
		}

		if (err == ERR_NOT_FOUND) {
			next_iter.node = 0;
			next_iter.has_node = false;
			break;
		}

		return err;
	}

	*(riscv_hal_mem_iter_t*)iter = next_iter;
	return ERR_NOT_FOUND;
}
