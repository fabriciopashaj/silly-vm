#include "silly_common.h"
#include <malloc.h>
#include <string.h>

int silly_stack_init(SillyStack *stack, size_t slot_count,
						size_t register_count, void *symbol)
{
	stack->size			  = slot_count * sizeof(SillyStackSlot);
	stack->slot_count	  = slot_count;
	stack->count		  = 0;
	stack->register_count = register_count;
	stack->data			  = (SillyStackSlot *)malloc(stack->size);

	if (stack->data == NULL)
		return SILLY_S_NO_MEMORY;

	stack->data_start	  = stack->data + register_count;
	stack->stack_top	  = stack->data_start;
	stack->symbol		  = symbol;
	stack->parent		  = NULL;
	stack->next			  = NULL;

	return SILLY_S_OK;
}
int silly_stack_push(SillyStack *stack, SillyStackSlot *value)
{
	if (stack->count == SILLY_MAX_STACK_COUNT)
	{
		if (stack->count == SILLY_MAX_STACK_COUNT)
			return SILLY_S_STACK_OVERFLOW;
		else
		{
			stack->data = (SillyStackSlot *)realloc(stack->data,
													SILLY_MAX_STACK_SIZE);
			if (stack->data == NULL)
				return SILLY_S_NO_MEMORY;
		}
	}
	memcpy(stack->stack_top, value, sizeof(SillyStackSlot));
	stack->stack_top++;
	stack->count++;
	return SILLY_S_OK;
}
int silly_stack_pop(SillyStack *stack, int type,
							   SillyStackSlot *target)
{
	if (stack->count == 0)
		return SILLY_S_STACK_EMPTY;
	else
	{
		if (type != stack->stack_top[-1].type)
			return SILLY_S_STACK_POP_MISMATCH;
		memcpy(target, (stack->stack_top = &stack->stack_top[-1]),
				sizeof(SillyStackSlot));
		stack->count--;
		return SILLY_S_OK;
	}
}
int silly_stack_drop(SillyStack *stack)
{
	if (stack->count == 0)
		return SILLY_S_STACK_EMPTY;
	memset(--(stack->stack_top), 0, sizeof(SillyStackSlot));
	stack->count--;
	return SILLY_S_OK;
}
// XXX: Call silly_stack_link with the parent and next stack.
SillyStack silly_stack_split(SillyStack *stack, SillyType *type,
		void *symbol)
{
	SillyStack new_stack;
	new_stack.count			 = 0;
	new_stack.slot_count	 = stack->slot_count -
								stack->register_count - stack->count +
								type->param_count;
	new_stack.size			 = new_stack.slot_count * sizeof(SillyStackSlot);
	new_stack.register_count = type->param_count + type->local_count;
	new_stack.symbol		 = symbol;
	new_stack.data			 = stack->stack_top - type->param_count;
	new_stack.data_start	 = stack->stack_top + type->local_count;
	new_stack.stack_top		 = new_stack.data_start;
	new_stack.next			 = NULL;
	return new_stack;
}
void silly_stack_dump(SillyStack *stack)
{
	stack->data_start = stack->stack_top = stack->stack_top;
	stack->count = 0;
	stack->register_count = 0;
	if (stack->next != NULL)
		silly_stack_clean(stack->next);
}
void silly_stack_clean(SillyStack *stack)
{
	if (stack != NULL)
	{
		if (stack->data != NULL && stack->parent == NULL)
			free(stack->data);
		silly_stack_clean(stack->next);
		stack->data = stack->data_start = stack->stack_top = NULL;
		stack->register_count = 0;
		stack->size = 0;
		stack->count = 0;
		stack->parent = stack->next = NULL;
	}
}
int silly_memory_init(SillyMemory *memory, uint8_t page_count)
{
	if (page_count != 0)
	{
		memory->size = silly_memory_size_from_pages(page_count);
		memory->data = malloc(memory->size);
		if (memory->data == NULL)
			return SILLY_S_NO_MEMORY;
		
	}
	else
		memory->data = NULL;
	memory->page_count = page_count;
	return SILLY_S_OK;
}
int silly_memory_write(SillyMemory *memory, size_t offset, void *source,
						size_t size)
{
	if (memory->data == NULL)
		return SILLY_S_MEMORY_UNINITIALIZED;
	if (offset >= memory->size)
		return SILLY_S_MEMORY_ACCESS_OUT_OF_BOUNDS;
	if (size + offset >= memory->size)
		return SILLY_S_MEMORY_TO_BIG;
	memcpy(memory->data + offset, source, size);
	return SILLY_S_OK;
}
int silly_memory_read(SillyMemory *memory, size_t offset, void *dest,
						size_t size)
{
	if (memory->data == NULL)
		return SILLY_S_MEMORY_UNINITIALIZED;
	if (offset >= memory->size)
		return SILLY_S_MEMORY_ACCESS_OUT_OF_BOUNDS;
	if (size + offset >= memory->size)
		return SILLY_S_MEMORY_TO_BIG;
	memcpy(dest, memory->data + offset, size);
	return SILLY_S_OK;
}
void silly_memory_clean(SillyMemory *memory)
{
	if (memory != NULL)
	{
		if (memory->data != NULL)
			free(memory->data);
		memory->data = NULL;
		memory->page_count = 0;
		memory->size = 0;
	}
}
