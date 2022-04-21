#ifndef SILLY_H
#define SILLY_H
#include <stdlib.h>
#include <stdint.h>
#include <c-arena-alloc/lib.h>

// Typedefs of basic primitive types and structs
typedef uint8_t            U8;
typedef int8_t             S8;
typedef uint16_t           U16;
typedef int16_t            S16;
typedef unsigned long      U32;
typedef signed   long      S32;
typedef unsigned long long U64;
typedef signed   long long S64;
typedef          int       Int;
typedef unsigned int       Uint;
typedef signed   int       Sint;

typedef float      F32;
typedef double     F64;
typedef U32        Usize;
typedef S32        Ssize;
typedef char       *Str;
typedef char const *CStr;
typedef void       *voidptr;
typedef U8         STaggedValueKind;
typedef U8         Bool;

typedef struct sized_str_t {
  char const *str;
  Uint       len;
} SizedStr;

#ifdef DEBUG // so gdb shows us this instead of just integers during debug
#define E(e) SILLY_E_##e
typedef enum {
  E(OK) = 0,
  E(HALT),
  E(OOM),
  E(SO),
  E(CDE),
  E(CRASH),
  E(INCORRECT_SECTION_SIZE),
  E(INVALID_TYPE_SIZE),
  E(INCORRECT_TYPE_COUNT),
  E(INCORRECT_FUNCTION_COUNT),
  E(INVALID_TYPE_INDEX),
  E(EXHAUSTED_STACK),
  E(INVALID_OPERAND_KIND),
  E(OOB_LOCAL_ACCESS)
} SStatus;
#undef E
#else
#define SILLY_E_OK    0
#define SILLY_E_HALT  1
#define SILLY_E_OOM   2 // (O)ut (O)f (M)emory
#define SILLY_E_SO    3 // (S)tack (O)verflow
#define SILLY_E_CDE   4 // (C)all (D)epth (E)xceeded
#define SILLY_E_CRASH 5
// Errors that occur only during bytecode parsing
#define SILLY_E_INCORRECT_SECTION_SIZE   6
#define SILLY_E_INVALID_TYPE_SIZE        7
#define SILLY_E_INCORRECT_TYPE_COUNT     8
#define SILLY_E_INCORRECT_FUNCTION_COUNT 9
#define SILLY_E_INVALID_TYPE_INDEX       10
#define SILLY_E_EXHAUSTED_STACK          11
#define SILLY_E_INVALID_OPERAND_KIND     12
#define SILLY_E_OOB_LOCAL_ACCESS         13

typedef Ssize      SStatus;
#endif

// Internal VM structs

typedef struct SStackFrame SStackFrame;
typedef struct SCallFrame  SCallFrame;
typedef struct SEnv        SEnv;
typedef struct SStack      SStack;

typedef struct SType {
  U16       param_count;
  U16       result_count;
  U8  const *types;
} SType;

typedef struct SFunc {
  SType const *type;
  U8    const *code;
  U8    const *code_end;
  SizedStr    name;
  void        (*native)(SCallFrame *);
  struct {
    Uint code_offset; // from the start of the code section
    Uint code_end;
    Uint type_idx;
    Uint offset; // from the start of the function section
  }           raw;
  struct {
    Uint stack_use;
    Bool tail_opt;
    Bool recursive;
  }           meta;
  struct {
    U8  const *types;
    U16       count;
  }           locals;
} SFunc;
typedef ARENA_PTR_T(silly_func_ptr_t, SFunc) SFuncPtr;

// #include "functable.h"

typedef union SValue {
  U32 u32;
  S32 s32;
  F32 f32;
  U64 u64;
  S64 s64;
  F64 f64;
} SValue;

typedef struct STaggedValue {
  SValue           v;
  STaggedValueKind kind;
  U8               __padding[sizeof(U64) - sizeof(STaggedValueKind)];
} STaggedValue;

typedef struct SMemory {
  U32 page_count;
  U32 max_pages;
  U8  *data;
} SMemory;

struct SCallFrame {
  // the value at the stack top
  STaggedValue cache;
  // (s)tack (l)ocals, the pointer to where the locals start
  STaggedValue *sl;
  // (s)tack (b)ottom, the pointer to where the locals end and operational
  // stack where you push and pop starts
  STaggedValue *sb;
  // (s)tack (t)op, the pointer at the slot right after the last used one
  STaggedValue *st;
  // the pointer to the function that called
  SFunc        *function;
  SStack       *stack;
  U32          frame_index;
  // will tell at which opcode failure occurred if it occurs
  U8 const     *ip;
  // the size of the stack in STaggedValues it can fit
  U32          size;
};

#define SILLY_STACK_SIZE 0x800
#define SILLY_CALL_DEPTH 0x100
struct SStack {
  STaggedValue data  [SILLY_STACK_SIZE];
  SCallFrame   frames[SILLY_CALL_DEPTH];
  SCallFrame   *frame;
};

typedef struct SSecInfo {
  Uint size;
  Uint item_count;
} SSecInfo;

typedef struct SModule {
  CStr  path;
  struct {
    U8  *buffer;
    union {
      struct {
        SSecInfo type;
        SSecInfo func;
        SSecInfo data;
        SSecInfo code;
      }        seci;
      SSecInfo seci_arr[4];
    };
    struct {
      U16 min;
      U16 max;
    }   mem_cfg;
  }     raw;
  SType *types;
  SFunc *functions;
  Bool  initted;
} SModule;
typedef ARENA_PTR_T(silly_module_ptr_t, SModule) SModulePtr;

typedef struct AllocNode AllocNode;

struct SEnv {
  SStack     stack;
  SMemory    memory;
  ArenaAlloc mod_pool;
  SModule    *main_mod;
};

#endif /* SILLY_H */
