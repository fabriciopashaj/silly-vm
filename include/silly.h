#ifndef SILLY_H
#define SILLY_H
#include <stdlib.h>
#include <stdint.h>

// Defines
// Stack types
#define STYPE_U32 4
#define STYPE_S32 8
#define STYPE_U64 12
#define STYPE_S64 16
#define STYPE_F32 20
#define STYPE_F64 24

// Typedefs of basic primitive types and structs
typedef uint8_t    U8;
typedef int8_t     S8;
typedef uint16_t   U16;
typedef int16_t    S16;
typedef uint32_t   U32;
typedef int32_t    S32;
typedef uint64_t   U64;
typedef int64_t    S64;
typedef float      F32;
typedef double     F64;
typedef U32        Usize;
typedef S32        Ssize;
typedef char       *Str;
typedef char const *CStr;
typedef void       *voidptr;
typedef U8         SValueKind;

typedef struct PStr {
  U32 len;
  U8  str[1];
} PStr;

// Internal VM structs
typedef struct SType {
  U16 param_count;
  U16 result_count;
  U8  types[1];
} SType;

typedef struct SFunc {
  SType *type;
  U8    *code;
  U8    *code_end;
  PStr  *name;
  /* struct {
    Usize max : 15;
    Usize is_reported : 1;
  } stack_usage; */
  // void  (*native)(SCallFrame *);
} SFunc;
typedef SFunc **SFuncTable;

// A stack value
typedef struct SValue {
  union {
    U32 u32;
    S32 s32;
    F32 f32;
    U64 u64;
    S64 s64;
    F64 f64;
  };
  SValueKind kind;
  U8 __padding[16 - sizeof(U64) - sizeof(SValueKind)];
} SValue;

typedef struct SStackFrame SStackFrame;
typedef struct SCallFrame  SCallFrame;
typedef struct SEnv        SEnv;

typedef struct SStack {
  SValue *data;
  U32    size;
  U32    max_size;
  SCallFrame   *root_frame;
} SStack;

typedef struct SMemory {
  U32 page_count;
  U32 max_pages;
  U8  data;
} SMemory;

struct SCallFrame {
  // the value at the stack top
  SValue      cache;
  // (s)tack (l)ocals, the pointer to where the locals start
  SValue      *sl;
  // (s)tack (b)ottom, the pointer to where the locals end and operational
  // stack where you push and pop starts
  SValue      *sb;
  // (s)tack (t)op, the pointer at the slot right after the last used one
  SValue      *st;
  // the pointer to the function that called
  SFunc       *symbol;
  // the parent stack frame that contains this one, if it is NULL it means
  // that this is the root frame
  SStackFrame *parent;
  // the child frame that is nested inside this one
  SStackFrame *child;
  // the execution environment
  SEnv        *env;
  // the size of the stack in SValues it can fit
  U32         size;
};

typedef struct SModuleInfo SModuleInfo;

typedef struct SModule {
  CStr        path;
  SModuleInfo *raw;
  SType       **types;
  SFuncTable  function_table;
} SModule;

struct SEnv {
  SStack  stack;
  SMemory memory;
  SModule *modules;
  SFunc   *all_functions;;
};

#endif /* SILLY_H */
