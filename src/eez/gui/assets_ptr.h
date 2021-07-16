/*
 * EEZ Modular Firmware
 * Copyright (C) 2015-present, Envox d.o.o.
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

#pragma once

#include <type_traits>

#include <eez/memory.h>

namespace eez {

template<typename T, bool is_void>
struct AssetsPtrImpl;

/* This template is used (on 64-bit systems) to pack asset pointers into 32-bit values.
 * All pointers are relative to MEMORY_BEGIN.
 * This way, the assets created by Studio can be used without having to fix all
 * the sizes - Studio creates 32-bit pointers that are relative to the
 * beginning of the assets, which the firmware rewrites to global pointers
 * during initialization. On a 32-bit system this works just fine, but for a
 * 64-bit system the pointers have different sizes and this breaks. By
 * inserting a 'middleman' structure that stores the pointers as a 32-bit
 * offset to MEMORY_BEGIN, we can keep the pointer sizes and initialization
 * code the same.
 */
template<typename T>
struct AssetsPtrImpl<T, false>
{
    /* Default constructors initialize to an invalid pointer */
    AssetsPtrImpl() = default;
    AssetsPtrImpl(std::nullptr_t v) {};
    /* Can accept void or T pointers. Store the offset to MEMORY_BEGIN. */
    AssetsPtrImpl(T * p) : offset(reinterpret_cast<const uint8_t*>(p) - MEMORY_BEGIN) {}
    AssetsPtrImpl(void * p) : offset(reinterpret_cast<const uint8_t*>(p) - MEMORY_BEGIN) {}

    /* Conversion to an int return the raw value */
    operator uint32_t() const { return offset; }
    
    /* Implicit conversion to a T pointer */
    operator T*() const { return ptr(); }
    
    /* Implicit conversions to void pointers */
    operator       void*() const { return ptr(); }
    operator const void*() const { return ptr(); }

    /* Special cases for converting to (unsigned) char for pointer arithmetic */
    template<typename U, typename = typename std::enable_if<!std::is_same<typename std::remove_pointer<U>::type, T>::value>>
    operator U() const { return (U)ptr(); }
    
    /* Dereferencing operators */
          T* operator->()       { return ptr(); }
    const T* operator->() const { return ptr(); }
    
    /* Array access */
    T&       operator[](uint32_t i)       { return ptr()[i]; }
    const T& operator[](uint32_t i) const { return ptr()[i]; }

	T&       operator[](int i) { return ptr()[i]; }
	const T& operator[](int i) const { return ptr()[i]; }

	T&       operator[](int16_t i) { return ptr()[i]; }
	const T& operator[](int16_t i) const { return ptr()[i]; }


    /* Implement addition of ints just like 'normal' pointers */
    T* operator+(int i)      { return &ptr()[i]; }
    T* operator+(unsigned i) { return &ptr()[i]; }

    /* Calculate the pointer, return a null pointer if the value is invalid */
    T* ptr() const {
        if (offset == 0xFFFFFFFF) {
            return 0;
        } else {
            return (T*)(MEMORY_BEGIN + offset);
        }
    }

    uint32_t offset{0xFFFFFFFF};
};

/* Template specialization for void pointers that does not allow dereferencing */
template<typename T>
struct AssetsPtrImpl<T, true>
{
    AssetsPtrImpl() = default;
    AssetsPtrImpl(std::nullptr_t) {};
    AssetsPtrImpl(T* p) : offset(reinterpret_cast<const uint8_t*>(p) - MEMORY_BEGIN) {}
    operator uint32_t() const { return offset; }
    operator T*() const {
		if (offset == 0xFFFFFFFF) {
			return 0;
		} else {
			return (T*)(MEMORY_BEGIN + offset);
		}
	}

    /* Implicit conversions to convert to any pointer type */
    template<typename U> operator U() const { return (U)(T*)(*this); }
    uint32_t offset{0xFFFFFFFF};
};

/* This struct chooses the type used for AssetsPtr<T> - by default it uses an AssetsPtrImpl<> */
template<typename T, uint32_t ptrSize>
struct AssetsPtrChooser
{
    using type = AssetsPtrImpl<T, std::is_void<T>::value>;
};

//#if defined(EEZ_PLATFORM_STM32)

/* On 32 bit systems, we can just use raw pointers */
template<typename T>
struct AssetsPtrChooser<T, 4>
{
    using type = T*;
};

//#endif

/* Utility typedef that delegates to AssetsPtrChooser */
template<typename T>
using AssetsPtr = typename AssetsPtrChooser<T, sizeof(void*)>::type;

} // namespace eez
