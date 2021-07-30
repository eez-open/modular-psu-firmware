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

#include <eez/alloc.h>
#include <eez/system.h>

namespace eez {

static const size_t ALIGNMENT = 8;
static const size_t MIN_BLOCK_SIZE = 8;

struct AllocBlock {
	AllocBlock *next;
	int free;
	size_t size;
};

static uint8_t *g_heap;

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wparentheses"
#endif
osMutexId(g_mutexId);
#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic pop
#endif
osMutexDef(g_mutex);

void initAllocHeap(uint8_t *heap, size_t heapSize) {
    g_heap = heap;

	AllocBlock *first = (AllocBlock *)g_heap;
	first->next = 0;
	first->free = 1;
	first->size = heapSize - sizeof(AllocBlock);

	g_mutexId = osMutexCreate(osMutex(g_mutex));
}

void *alloc(size_t size) {
	if (size == 0) {
		return nullptr;
	}

	size = ((size + ALIGNMENT - 1) / ALIGNMENT) * ALIGNMENT;

	AllocBlock *first = (AllocBlock *)g_heap;

	AllocBlock *block = first;

	if (osMutexWait(g_mutexId, osWaitForever) == osOK) {
		while (block) {
			if (block->free && block->size >= size) {
				break;
			}
			block = block->next;
		}

		if (!block) {
			return nullptr;
		}

		int nextBlockSize = block->size - size - sizeof(AllocBlock);
		if (nextBlockSize >= (int)MIN_BLOCK_SIZE) {
			auto nextBlock = (AllocBlock *)((uint8_t *)block + sizeof(AllocBlock) + size);
			nextBlock->next = block->next;
			nextBlock->free = 1;
			nextBlock->size = nextBlockSize;
			block->next = nextBlock;
			block->size = size;
		}

		block->free = 0;

		osMutexRelease(g_mutexId);
	} else {
		osDelay(1);
	}
		
	return block + 1;    
}

void free(void *ptr) {
	if (ptr == 0) {
		return;
	}

	AllocBlock *first = (AllocBlock *)g_heap;

	AllocBlock *block = first;

	if (osMutexWait(g_mutexId, osWaitForever) == osOK) {
		AllocBlock *prevBlock = nullptr;
		while (block && block + 1 < ptr) {
			prevBlock = block;
			block = block->next;
		}

		if (!block || block + 1 != ptr) {
			// assert(0);
			return;
		}

		if (block->next && block->next->free) {
			if (prevBlock && prevBlock->free) {
				prevBlock->next = block->next->next;
				prevBlock->size += sizeof(AllocBlock) + block->size + sizeof(AllocBlock) + block->next->size;
			} else {
				block->size += sizeof(AllocBlock) + block->next->size;
				block->next = block->next->next;
				block->free = 1;
			}
		} else if (prevBlock && prevBlock->free) {
			prevBlock->next = block->next;
			prevBlock->size += sizeof(AllocBlock) + block->size;
		} else {
			block->free = 1;
		}
		block->free = 1;

		osMutexRelease(g_mutexId);
	} else {
		osDelay(1);
	}
}

template<typename T> T *allocObject() {
	void *ptr = alloc(sizeof(T));
	return new (ptr) T;
}

template<typename T> void freeObject(T *ptr) {
	ptr->~T();
	free(ptr);
}

void dumpAlloc(scpi_t *context) {
	AllocBlock *first = (AllocBlock *)g_heap;
	AllocBlock *block = first;
	while (block) {
		char buffer[100];
		snprintf(buffer, sizeof(buffer), "%s: %d", block->free ? "FREE" : "ALOC", block->size);
		SCPI_ResultText(context, buffer);
		block = block->next;
	}
}

} // eez
