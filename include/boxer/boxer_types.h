/*
 * boxer_types.h - Type definitions for Boxer-DOSBox integration
 *
 * This header defines types shared between Boxer and DOSBox Staging.
 * It includes forward declarations and platform-specific types needed
 * by the Boxer hook infrastructure.
 *
 * Copyright (c) 2013 Alun Bestor and contributors. All rights reserved.
 * This source file is released under the GNU General Public License 2.0.
 */

#ifndef BOXER_TYPES_H
#define BOXER_TYPES_H

#ifdef BOXER_INTEGRATED

#include <cstdint>
#include <cstddef>

// ============================================================================
// Forward Declarations for DOSBox Types
// ============================================================================

// Forward declare DOSBox types to avoid circular dependencies
class DOS_Drive;
class DOS_Shell;

// ============================================================================
// Forward Declarations for SDL Types
// ============================================================================

// SDL structures used in rendering callbacks
struct SDL_Window;
struct SDL_Surface;

// ============================================================================
// DOSBox Primitive Type Aliases
// ============================================================================

// Legacy DOSBox used custom integer types - map to standard types
// These may be defined elsewhere in DOSBox, but we include them here
// to ensure the headers compile standalone

#ifndef DOSBOX_TYPES_DEFINED
#define DOSBOX_TYPES_DEFINED

// 8-bit types
typedef uint8_t  Bit8u;
typedef int8_t   Bit8s;

// 16-bit types
typedef uint16_t Bit16u;
typedef int16_t  Bit16s;

// 32-bit types
typedef uint32_t Bit32u;
typedef int32_t  Bit32s;

// 64-bit types
typedef uint64_t Bit64u;
typedef int64_t  Bit64s;

// Size type (pointer-sized unsigned integer)
typedef size_t Bitu;
typedef ptrdiff_t Bits;

#endif // DOSBOX_TYPES_DEFINED

// ============================================================================
// Graphics Callback Types
// ============================================================================

// Graphics callback function pointer type
// Used by GFX_SetSize to notify when frame dimensions change
// Note: Named Boxer_GFX_CallBack_t to avoid conflict with legacy DOSBox GFX_CallBack_t
typedef void (*Boxer_GFX_CallBack_t)(Bitu width, Bitu height);

// ============================================================================
// File I/O Types
// ============================================================================

// File statistics structure (standard POSIX stat)
#include <sys/stat.h>

// Directory handle type
typedef void* DIR_Handle;

#endif // BOXER_INTEGRATED

#endif // BOXER_TYPES_H
