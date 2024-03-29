#include "parser.h"
#include "internal.h"
#include "code.h"
#include "macros.h"
#include "code.h"
#include "util.h"
#define throw(err) { status = SILLY_E_##err; goto catch; }
#define throw_if(cond, err) if (cond) { throw(err); }
#define try(s) if ((status = (s)) != SILLY_E_OK) { goto catch; }

#define SECTION_PARSER(nm)                                            \
  SStatus parse_##nm##_section(SEnv *env, SModule *mod,               \
                               U8 const *const ptr, SSecInfo const info)

/**
 */
SECTION_PARSER(cpool)
{
  SStatus status = SILLY_E_OK;
  U32 const *pool_bin = (U32 const *)ptr;
  mod->cpools.i64.size = LOAD_U32(pool_bin++);
  mod->cpools.f64.size = LOAD_U32(pool_bin++);
  mod->cpools.i32.size = LOAD_U32(pool_bin++);
  mod->cpools.f32.size = LOAD_U32(pool_bin++);

  mod->cpools.i64.data = (U64 *)pool_bin;
  mod->cpools.f64.data = (F64 *)(mod->cpools.i64.data +
                                 mod->cpools.i64.size);
  mod->cpools.i32.data = (U32 *)(mod->cpools.f64.data +
                                 mod->cpools.f64.size);
  mod->cpools.f32.data = (F32 *)mod->cpools.i32.data +
                                mod->cpools.i32.size;
  // We convert the values from little to big endian
#if SILLY_LITTLE_ENDIAN == 0
  for (U8 *p = mod->cpools.i64.data;
       (void *)p < mod->cpools.i32.data; p += sizeof(U64))
  {
    U8 tmp = p[7];
    p[7]   = p[0];
    p[0]   = tmp;
    tmp    = p[6];
    p[6]   = p[1];
    p[1]   = tmp;
    tmp    = p[5];
    p[5]   = p[2];
    p[2]   = tmp;
    tmp    = p[4];
    p[4]   = p[3];
    p[3]   = tmp;
  }
  for (U8 *p = mod->cpools.i32.data,
       end = (mod->cpools.f32.data + mod->cpools.f32.size);
       p < end; p += sizeof(U32))
  {
    U8 tmp = p[3];
    p[3]   = p[0];
    p[0]   = tmp;
    tmp    = p[2];
    p[2]   = p[1];
    p[1]   = tmp;
  }
#endif /* SILLY_LITTLE_ENDIAN == 0 */
  return status;
}

/**
 * @fn SStatus parse_type_section(SEnv *env, SModule *mod,
 *                                U8 const *const ptr,
 *                                SSecInfo const info)
 * @brief Parses the type section of a binary module.
 */
SECTION_PARSER(type)
{
  SStatus status = SILLY_E_OK;
  U8 const *const end = ptr + info.size;
  U8 const *type_ptr = ptr;
  Uint idx = 0;
  for (; idx < info.item_count && type_ptr < end; ++idx)
  {
    SType *type = STypeVec_push_empty(&(mod->types));
    Uint param_count = LOAD_U16(type_ptr);
    type_ptr += sizeof(U16);
    Uint result_count = LOAD_U16(type_ptr);
    type_ptr += sizeof(U16);
    Uint types_count = param_count + result_count;

    // TODO: Put some size checks so we don't read memory outside buffer
    //     : TODO: make it optional for custom builds for max startup
    //             speed and shit like that
    type->param_count  = param_count;
    type->result_count = result_count;
    type->types = type_ptr;

    type_ptr += types_count;
    // TODO: Create some more detailed error reporting system
    throw_if(type_ptr > end, INVALID_TYPE_SIZE);
  }
  throw_if(type_ptr != end, INCORRECT_SECTION_SIZE);
  throw_if(idx != info.item_count, INCORRECT_TYPE_COUNT);
catch:
  if (status != SILLY_E_OK)
  {
  }
  return status;
}

// TODO: Figure out how data section should be used and loaded
/**
 * @fn SStatus parse_data_section(SEnv *env, SModule *mod,
 *                                U8 const *const ptr,
 *                                SSecInfo const info)
 * @brief Parses the data section of a binary module.
 */
SECTION_PARSER(data)
{
  SStatus status = SILLY_E_OK;

  LOG_ERROR("Parser of data section is not implemented\n");
  exit(1);

// catch:
  return status;
}

/**
 * @fn SStatus parse_function_section(SEnv *env, SModule *mod,
 *                                    U8 const *const ptr,
 *                                    SSecInfo const info)
 * @brief Parses the function section of a binary module.
 */
SECTION_PARSER(function)
{
  SStatus status = SILLY_E_OK;
  void const *const end = ptr + info.size;
  void const *funcd_ptr = ptr;
  Uint idx = 0;
  for (; idx < info.item_count && funcd_ptr < end; ++idx)
  {
    SFunc *func = SFuncVec_push_empty(&(mod->functions));
    func->raw.offset = funcd_ptr - (void const *)ptr; // buffer;
    func->raw.type_idx = LOAD_U32(funcd_ptr);
    funcd_ptr += sizeof(U32);
    throw_if((Int)func->raw.type_idx >= STypeVec_len(mod->types),
             INVALID_TYPE_INDEX);
    func->type = &(mod->types[func->raw.type_idx]);

    func->raw.code_offset = LOAD_U32(funcd_ptr);
    funcd_ptr += sizeof(U32);

    func->raw.code_size = LOAD_U32(funcd_ptr);
    funcd_ptr += sizeof(U32);

    // TODO: Check for overflows here
    func->locals.count = LOAD_U16(funcd_ptr);
    funcd_ptr += sizeof(U16);
    func->locals.types = func->locals.count == 0 ? NULL : funcd_ptr;
    funcd_ptr += func->locals.count;

    func->name.len = LOAD_U8(funcd_ptr++);
    func->name.str = funcd_ptr;
    funcd_ptr += func->name.len;
  }
  throw_if(funcd_ptr != end,       INCORRECT_SECTION_SIZE);
  throw_if(idx != info.item_count, INCORRECT_FUNCTION_COUNT);
catch:
  return status;
}

#define CVECTOR_POINTERMODE
#include <c-vector/lib.h>
CVECTOR_WITH_NAME(STaggedValueKind, PseudoStack);

/**
 * @brief  Validates the bytecode body of a function.
 * @param  func The function that'll be validated
 * @param  mod  The module to which the function belongs to
 * @param  bytc The pointer to the bytecode
 * @return The status of execution
 */
static SStatus validate_function(SFunc *func, SModule *mod,
                                 U8 const *bytc)
{
#define CHECK_SIZE()                \
  do {                              \
    Uint l = PseudoStack_len(stack);\
    if (l > max_stack)              \
    {                               \
      max_stack = l;                \
    }                               \
  } while (0);
  SStatus status = SILLY_E_OK;
  PseudoStack stack = PseudoStack_with_capacity(0x100);
  Uint max_stack = 0;
  Uint size = func->raw.code_size;
  U8 const *const end = bytc + size;
  for (U8 const *ip = bytc; ip < end; )
  {
    U8 opc = *(ip++);
    STaggedValueKind opd_kind;
    switch (opc)
    {
#define OP(op) case SILLY_INSTR_##op:
#define OP_RANGE(s, e) case SILLY_INSTR_##s...SILLY_INSTR_##e:
      OP(DUPLICATE_ANY)
      {
        PseudoStack_push(&stack, stack[PseudoStack_len(stack) - 1]);
        CHECK_SIZE();
        break;
      }
      OP_RANGE(CONST_I32, CONST_F64)
      {
        Uint pooli = opc - SILLY_INSTR_CONST_I32;
        PseudoStack_push(&stack, (pooli + 1) << 2);
        CHECK_SIZE();
        Uint index = LOAD_U32(ip);
        throw_if(index >= mod->cpool_arr[pooli].size, CAOOB);
        ip += 4;
        break;
      }
      OP(SET_LOCAL) OP(GET_LOCAL)
      {
        PseudoStack_pop(stack);
        Uint i = LOAD_U32(ip);
        throw_if(i >= func->type->param_count + func->locals.count,
                 OOB_LOCAL_ACCESS
                 /* TODO: Make new errors and try to make some decent
                  * working error reporting mesage (probably later) */);
        ip += 4;
        break;
      }
      OP_RANGE(ADD_I32, RSHIFT_I32)
      {
        opd_kind = SILLY_TYPE_I32;
        goto binop_operand_check;
      }
      OP_RANGE(ADD_I64, RSHIFT_I64)
      {
        opd_kind = SILLY_TYPE_I64;
        goto binop_operand_check;
      }
      OP_RANGE(ADD_F32, REM_F32)
      {
        opd_kind = SILLY_TYPE_F32;
        goto binop_operand_check;
      }
      OP_RANGE(FLOOR_F32, SQRT_F32)
      {
        opd_kind = SILLY_TYPE_F32;
        goto unop_operand_check;
      }
      OP_RANGE(ADD_F64, REM_F64)
      {
        opd_kind = SILLY_TYPE_F64;
binop_operand_check:
        throw_if(PseudoStack_len(stack) < 2, EXHAUSTED_STACK);
        STaggedValueKind opd1_kind = stack[PseudoStack_len(stack) - 2];
        STaggedValueKind opd2_kind = stack[PseudoStack_len(stack) - 1];
        throw_if(opd1_kind != opd_kind, INVALID_OPERAND_KIND);
        throw_if(opd2_kind != opd_kind, INVALID_OPERAND_KIND);
        PseudoStack_pop(stack);
        break;
      }
      OP_RANGE(FLOOR_F64, SQRT_F64)
      {
        opd_kind = SILLY_TYPE_F64;
unop_operand_check:
        throw_if(PseudoStack_len(stack) < 1, EXHAUSTED_STACK);
        throw_if(stack[PseudoStack_len(stack) - 1] != opd_kind,
                 INVALID_OPERAND_KIND);
        break;
      }
#undef OP_RANGE
#undef OP
    }
  }
  throw_if(PseudoStack_len(stack) != func->type->result_count,
           INVALID_RESULTS);
  throw_if(memcmp(stack, func->type->types,
                  func->type->result_count) != 0,
           INVALID_RESULT_TYPES);
  func->meta.stack_use = max_stack;
catch:
  return status;
}

/**
 * @fn SStatus parse_code_section(SEnv *env, SModule *mod,
 *                                U8 const *const ptr,
 *                                SSecInfo const info)
 * @brief Parses the code section of a binary module.
 */
SECTION_PARSER(code)
{
  SStatus status = SILLY_E_OK;

  Uint i = 0;
  for (SFunc *func = mod->functions,
             *end = func + SFuncVec_len(mod->functions);
       func < end; ++func, ++i)
  {
    try(validate_function(func, mod, &(ptr[func->raw.code_offset])));
  }
catch:
  return status;
}

#if 0
SStatus parse_module(SEnv *env, SModule *mod)
{
  SStatus status = SILLY_S_OK;
  U8 *ptr = mod->raw.buffer;
  U32 len = Vector_U8_len(ptr);
  U8 *buff_end = ptr + len;
  try_eq(len, sizeof(mod->raw) + 8, MODULE_BELOW_MIN_SIZE);
  try_eq(memcmp(ptr, "\0SillyVM", 8), 0, MODULE_INCORRECT_SIGNATURE);
  mod->raw.size = len;
  ptr += 8;
  U32 type_sec_size = READ_U32(((U32 *)ptr)++);
  U32 func_sec_size = READ_U32(((U32 *)ptr)++);
  U32 code_sec_size = READ_U32(((U32 *)ptr)++);
  U32 data_sec_size = READ_U32(((U32 *)ptr)++);
  U8 const *calculated_end = ptr + type_sec_size;
  try_le(calculated_end, buff_end, OOB_SECTION_SIZE);
  try_le(calculated_end += func_sec_size, buff_end, OOB_SECTION_SIZE);
  try_le(calculated_end += code_sec_size, buff_end, OOB_SECTION_SIZE);
  try_le(calculated_end += data_sec_size, buff_end, OOB_SECTION_SIZE);

  _(parse_type_section(env, mod, &ptr, type_sec_size));
  _(parse_func_section(env, mod, &ptr, func_sec_size));
  _(parse_data_section(env, mod, &ptr, data_sec_size));
  _(parse_code_section(env, mod, &ptr, code_sec_size));

catch:
  return status;
}
#endif
