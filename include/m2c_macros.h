#ifndef M2C_MACROS_H
#define M2C_MACROS_H

/* Definitions for the placeholders that m2c --valid-syntax emits, so its
   raw output compiles. Decompiled sources should not depend on this file. */

typedef s32 M2C_UNK;
typedef s8 M2C_UNK8;
typedef s16 M2C_UNK16;
typedef s32 M2C_UNK32;
typedef s64 M2C_UNK64;

#define M2C_ERROR(desc) (0)
#define M2C_TRAP_IF(cond) (0)
#define M2C_BREAK() (0)
#define M2C_SYNC() (0)
#define M2C_CARRY 0
#define M2C_FIELD(expr, type, offset) (*(type)((s8*)(expr) + (offset)))
#define M2C_BITWISE(type, expr) ((type)(expr))
#define M2C_STRUCT_COPY(dst, src) (*(dst) = *(src))
#define M2C_LWL(expr) (expr)
#define M2C_FIRST3BYTES(expr) (expr)
#define M2C_UNALIGNED32(expr) (expr)
#define M2C_OVERFLOW(expr) (0)
#define M2C_MEMCPY_ALIGNED bcopy
#define M2C_MEMCPY_UNALIGNED bcopy

#endif
