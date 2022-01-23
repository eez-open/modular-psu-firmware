/*
 * EEZ Modular Firmware
 * Copyright (C) 2021-present, Envox d.o.o.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <math.h>
#include <assert.h>

#include <eez/core/alloc.h>
#include <eez/core/os.h>

namespace eez {

static const size_t ALIGNMENT = 8;
static const size_t MIN_BLOCK_SIZE = 8;

struct AllocBlock {
	AllocBlock *next;
	int free;
	size_t size;
	uint32_t id;
};

static uint8_t *g_heap;

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wparentheses"
#endif

EEZ_MUTEX_DECLARE(alloc);

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic pop
#endif

void initAllocHeap(uint8_t *heap, size_t heapSize) {
    g_heap = heap;

	AllocBlock *first = (AllocBlock *)g_heap;
	first->next = 0;
	first->free = 1;
	first->size = heapSize - sizeof(AllocBlock);

	EEZ_MUTEX_CREATE(alloc);
}

void *alloc(size_t size, uint32_t id) {
	if (size == 0) {
		return nullptr;
	}

	if (EEZ_MUTEX_WAIT(alloc, 0)) {
		AllocBlock *firstBlock = (AllocBlock *)g_heap;

		AllocBlock *block = firstBlock;
		size = ((size + ALIGNMENT - 1) / ALIGNMENT) * ALIGNMENT;

		// find free block with enough size
		// TODO merge multiple free consecutive blocks into one that has enough size
		while (block) {
			if (block->free && block->size >= size) {
				break;
			}
			block = block->next;
		}

		if (!block) {
			EEZ_MUTEX_RELEASE(alloc);
			return nullptr;
		}

		int remainingSize = block->size - size - sizeof(AllocBlock);
		if (remainingSize >= (int)MIN_BLOCK_SIZE) {
			// remainingSize is enough to create a new block
			auto newBlock = (AllocBlock *)((uint8_t *)block + sizeof(AllocBlock) + size);
			newBlock->next = block->next;
			newBlock->free = 1;
			newBlock->size = remainingSize;

			block->next = newBlock;
			block->size = size;
		}

		block->free = 0;
		block->id = id;

		EEZ_MUTEX_RELEASE(alloc);

		return block + 1;
	}

	return nullptr;
}

void free(void *ptr) {
	if (ptr == 0) {
		return;
	}

	if (EEZ_MUTEX_WAIT(alloc, 0)) {
		AllocBlock *firstBlock = (AllocBlock *)g_heap;

		AllocBlock *prevBlock = nullptr;
		AllocBlock *block = firstBlock;
		
		while (block && block + 1 < ptr) {
			prevBlock = block;
			block = block->next;
		}

		if (!block || block + 1 != ptr || block->free) {
			assert(false);
			EEZ_MUTEX_RELEASE(alloc);
			return;
		}

		// reset memory to catch errors when memory is used after free is called
		memset(ptr, 0xCC, block->size);

		auto nextBlock = block->next;
		if (nextBlock && nextBlock->free) {
			if (prevBlock && prevBlock->free) {
				// both next and prev blocks are free, merge 3 blocks into one
				prevBlock->next = nextBlock->next;
				prevBlock->size += sizeof(AllocBlock) + block->size + sizeof(AllocBlock) + nextBlock->size;
			} else {
				// next block is free, merge 2 blocks into one
				block->next = nextBlock->next;
				block->size += sizeof(AllocBlock) + nextBlock->size;
				block->free = 1;
			}
		} else if (prevBlock && prevBlock->free) {
			// prev block is free, merge 2 blocks into one
			prevBlock->next = nextBlock;
			prevBlock->size += sizeof(AllocBlock) + block->size;
		} else {
			// just free
			block->free = 1;
		}

		EEZ_MUTEX_RELEASE(alloc);
	}
}

template<typename T> void freeObject(T *ptr) {
	ptr->~T();
	free(ptr);
}

#if OPTION_SCPI
void dumpAlloc(scpi_t *context) {
	AllocBlock *first = (AllocBlock *)g_heap;
	AllocBlock *block = first;
	while (block) {
		char buffer[100];
		if (block->free) {
			snprintf(buffer, sizeof(buffer), "FREE: %d", block->size);
		} else {
			snprintf(buffer, sizeof(buffer), "ALOC (0x%08x): %d", (unsigned int)block->id, (int)block->size);
		}
		SCPI_ResultText(context, buffer);
		block = block->next;
	}
}
#endif

void getAllocInfo(uint32_t &free, uint32_t &alloc) {
	free = 0;
	alloc = 0;
	if (EEZ_MUTEX_WAIT(alloc, 0)) {
		AllocBlock *first = (AllocBlock *)g_heap;
		AllocBlock *block = first;
		while (block) {
			if (block->free) {
				free += block->size;
			} else {
				alloc += block->size;
			}
			block = block->next;
		}
		EEZ_MUTEX_RELEASE(alloc);
	}
}

} // eez
