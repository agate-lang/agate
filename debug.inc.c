// SPDX-License-Identifier: MIT
// Copyright (c) 2013-2021 Robert Nystrom and Wren Contributors
//Copyright (c) 2022 Julien Bernard

static void agateFunctionPrint(AgateFunction *function) {
  if (function->name == NULL) {
    printf("<script>");
    return;
  }

  printf("<func %s>", function->name->data);
}

static void agateEntityPrint(AgateValue value) {
  switch (agateEntityKind(value)) {
    case AGATE_ENTITY_ARRAY:
      printf("array");
      break;
    case AGATE_ENTITY_CLASS:
      printf("%s", agateAsClass(value)->name->data);
      break;
    case AGATE_ENTITY_CLOSURE:
      agateFunctionPrint(agateAsClosure(value)->function);
      break;
    case AGATE_ENTITY_FOREIGN:
      printf("foreign");
      break;
    case AGATE_ENTITY_FUNCTION:
      agateFunctionPrint(agateAsFunction(value));
      break;
    case AGATE_ENTITY_INSTANCE:
      printf("instance");
      break;
    case AGATE_ENTITY_MAP:
      printf("map");
      break;
    case AGATE_ENTITY_RANDOM:
      printf("random");
      break;
    case AGATE_ENTITY_RANGE:
      printf("range");
      break;
    case AGATE_ENTITY_STRING:
      printf("\"%s\"", agateAsCString(value));
      break;
    case AGATE_ENTITY_UNIT:
      printf("unit");
      break;
    case AGATE_ENTITY_UPVALUE:
      printf("upvalue");
      break;
  }
}

static void agateValuePrint(AgateValue value) {
  switch (value.kind) {
    case AGATE_VALUE_UNDEFINED:
      printf("undefined");
      break;
    case AGATE_VALUE_NIL:
      printf("nil");
      break;
    case AGATE_VALUE_BOOL:
      printf("%s", agateAsBool(value) ? "true" : "false");
      break;
    case AGATE_VALUE_CHAR:
      printf("U+%" PRIX32, agateAsChar(value));
      break;
    case AGATE_VALUE_INT:
      printf("%" PRIi64 , agateAsInt(value));
      break;
    case AGATE_VALUE_FLOAT:
      printf("%g", agateAsFloat(value));
      break;
    case AGATE_VALUE_ENTITY:
      agateEntityPrint(value);
      break;
  }
}

#if defined(AGATE_DEBUG_PRINT_CODE) || defined(AGATE_DEBUG_TRACE_EXECUTION)
static inline uint16_t agateDisassembleRead16(const AgateBytecode *bc, ptrdiff_t offset) {
  return (bc->code.data[offset] << 8) | bc->code.data[offset + 1];
}

static ptrdiff_t agateDisassembleSimpleInstruction(const char *name, ptrdiff_t offset) {
  printf("%s\n", name);
  return offset + 1;
}

static ptrdiff_t agateDisassembleConstantInstruction(const char *name, const AgateBytecode *bc, ptrdiff_t offset) {
  uint16_t constant = agateDisassembleRead16(bc, offset + 1);
  printf("%-16s %4d '", name, constant);
  assert((ptrdiff_t) constant <  bc->constants.size);
  agateValuePrint(bc->constants.data[constant]);
  printf("'\n");
  return offset + 3;
}

static ptrdiff_t agateDisassembleByteInstruction(const char *name, const AgateBytecode *bc, ptrdiff_t offset) {
  uint8_t slot = bc->code.data[offset + 1];
  printf("%-16s %4d\n", name, slot);
  return offset + 2;
}

static ptrdiff_t agateDisassembleShortInstruction(const char *name, const AgateBytecode *bc, ptrdiff_t offset) {
  uint16_t slot = agateDisassembleRead16(bc, offset + 1);
  printf("%-16s %4d\n", name, slot);
  return offset + 3;
}

static ptrdiff_t agateDisassembleJumpInstruction(const char *name, int sign, const AgateBytecode *bc, ptrdiff_t offset) {
  uint16_t jump = agateDisassembleRead16(bc, offset + 1);
  printf("%-16s %4td -> %td\n", name, offset, (ptrdiff_t) (offset + 3 + sign * jump));
  return offset + 3;
}

static ptrdiff_t agateDisassembleInstruction(AgateVM *vm, AgateFunction *function, ptrdiff_t offset) {
  AgateBytecode *bc = &function->bc;

  printf("%04td ", offset);

  int line = agateBytecodeLineFromOffset(bc, offset);

  if (offset > 0 && line == agateBytecodeLineFromOffset(bc, offset - 1)) {
    printf("   | ");
  } else {
    printf("%4d ", line);
  }

  uint8_t instruction = bc->code.data[offset];

  switch ((AgateOpCode) instruction) {
    case AGATE_OP_CONSTANT:
      return agateDisassembleConstantInstruction("CONSTANT", bc, offset);
    case AGATE_OP_NIL:
      return agateDisassembleSimpleInstruction("NIL", offset);
    case AGATE_OP_FALSE:
      return agateDisassembleSimpleInstruction("FALSE", offset);
    case AGATE_OP_TRUE:
      return agateDisassembleSimpleInstruction("TRUE", offset);
    case AGATE_OP_GLOBAL_LOAD:
    {
      uint16_t slot = agateDisassembleRead16(bc, offset + 1);
      printf("%-16s %4d '%s'\n", "GLOBAL_LOAD", slot, agateSymbolTableReverseFind(&function->unit->object_names, slot)->data);
      return offset + 3;
    }
    case AGATE_OP_GLOBAL_STORE:
    {
      uint16_t slot = agateDisassembleRead16(bc, offset + 1);
      printf("%-16s %4d '%s'\n", "GLOBAL_STORE", slot, agateSymbolTableReverseFind(&function->unit->object_names, slot)->data);
      return offset + 3;
    }
    case AGATE_OP_LOCAL_LOAD:
      return agateDisassembleByteInstruction("LOCAL_LOAD", bc, offset);
    case AGATE_OP_LOCAL_STORE:
      return agateDisassembleByteInstruction("LOCAL_STORE", bc, offset);
    case AGATE_OP_UPVALUE_LOAD:
      return agateDisassembleByteInstruction("UPVALUE_LOAD", bc, offset);
    case AGATE_OP_UPVALUE_STORE:
      return agateDisassembleByteInstruction("UPVALUE_STORE", bc, offset);
    case AGATE_OP_FIELD_LOAD:
      return agateDisassembleByteInstruction("FIELD_LOAD", bc, offset);
    case AGATE_OP_FIELD_STORE:
      return agateDisassembleByteInstruction("FIELD_STORE", bc, offset);
    case AGATE_OP_FIELD_LOAD_THIS:
      return agateDisassembleByteInstruction("FIELD_LOAD_THIS", bc, offset);
    case AGATE_OP_FIELD_STORE_THIS:
      return agateDisassembleByteInstruction("FIELD_STORE_THIS", bc, offset);
    case AGATE_OP_JUMP_FORWARD:
      return agateDisassembleJumpInstruction("JUMP_FORWARD", 1, bc, offset);
    case AGATE_OP_JUMP_BACKWARD:
      return agateDisassembleJumpInstruction("JUMP_BACKWARD", -1, bc, offset);
    case AGATE_OP_JUMP_IF:
      return agateDisassembleJumpInstruction("JUMP_IF", 1, bc, offset);
    case AGATE_OP_AND:
      return agateDisassembleJumpInstruction("AND", 1, bc, offset);
    case AGATE_OP_OR:
      return agateDisassembleJumpInstruction("OR", 1, bc, offset);
    case AGATE_OP_CALL:
      return agateDisassembleByteInstruction("CALL", bc, offset);
    case AGATE_OP_INVOKE:
    {
      uint8_t argc = bc->code.data[offset + 1];
      uint16_t slot = agateDisassembleRead16(bc, offset + 2);
      printf("%-16s %4d %d '%s'\n", "INVOKE", slot, argc, agateSymbolTableReverseFind(&vm->method_names, slot)->data);
      return offset + 4;
    }
    case AGATE_OP_SUPER:
    {
      uint8_t argc = bc->code.data[offset + 1];
      uint16_t slot1 = agateDisassembleRead16(bc, offset + 2);
      uint16_t slot2 = agateDisassembleRead16(bc, offset + 4);
      printf("%-16s %4d %4d %d\n", "SUPER", slot1, slot2, argc);
      return offset + 6;
    }
    case AGATE_OP_CLOSURE:
    {
      uint16_t constant = agateDisassembleRead16(bc, offset + 1);
      printf("%-16s %4d ", "CLOSURE", constant);
      agateValuePrint(bc->constants.data[constant]);
      printf("\n");

      AgateFunction *function = agateAsFunction(bc->constants.data[constant]);
      assert(agateIsFunction(bc->constants.data[constant]));

      offset += 3;

      for (ptrdiff_t j = 0; j < function->upvalue_count; ++j) {
        AgateCapture capture = bc->code.data[offset++];
        ptrdiff_t index = bc->code.data[offset++];
        printf("%04td      |                     %s %td\n", (ptrdiff_t) (offset - 2), capture == AGATE_CAPTURE_LOCAL ? "local" : "upvalue", index);
      }

      return offset;
    }
    case AGATE_OP_CLOSE_UPVALUE:
      return agateDisassembleSimpleInstruction("CLOSE_UPVALUE", offset);
    case AGATE_OP_POP:
      return agateDisassembleSimpleInstruction("POP", offset);
    case AGATE_OP_RETURN:
      return agateDisassembleSimpleInstruction("RETURN", offset);
    case AGATE_OP_CLASS:
      return agateDisassembleByteInstruction("CLASS", bc, offset);
    case AGATE_OP_CLASS_FOREIGN:
      return agateDisassembleSimpleInstruction("CLASS_FOREIGN", offset);
    case AGATE_OP_CONSTRUCT:
      return agateDisassembleSimpleInstruction("CONSTRUCT", offset);
    case AGATE_OP_CONSTRUCT_FOREIGN:
      return agateDisassembleSimpleInstruction("CONSTRUCT_FOREIGN", offset);
    case AGATE_OP_METHOD_INSTANCE:
      return agateDisassembleShortInstruction("METHOD_INSTANCE", bc, offset);
    case AGATE_OP_METHOD_CLASS:
      return agateDisassembleShortInstruction("METHOD_CLASS", bc, offset);
    case AGATE_OP_IMPORT_UNIT:
    {
      uint16_t name = agateDisassembleRead16(bc, offset + 1);
      printf("%-16s %4d ", "IMPORT_UNIT", name);
      agateValuePrint(bc->constants.data[name]);
      printf("\n");
      return offset + 3;
    }
    case AGATE_OP_IMPORT_OBJECT:
    {
      uint16_t variable = agateDisassembleRead16(bc, offset + 1);
      printf("%-16s %4d ", "IMPORT_OBJECT", variable);
      agateValuePrint(bc->constants.data[variable]);
      printf("\n");
      return offset + 3;
    }
    case AGATE_OP_END_UNIT:
      return agateDisassembleSimpleInstruction("END_UNIT", offset);
    case AGATE_OP_END:
      return agateDisassembleSimpleInstruction("END", offset);
    default:
      printf("Unknown opcode %d\n", instruction);
      assert(false);
      return offset + 1;
  }

  assert(false);
  return offset;
}
#endif

#ifdef AGATE_DEBUG_PRINT_CODE
static void agateDisassemble(AgateVM *vm, AgateFunction *function) {
  printf("== %s ==\n", function->name ? function->name->data : "<script>");

  for (ptrdiff_t offset = 0; offset < function->bc.code.size;) {
    offset = agateDisassembleInstruction(vm, function, offset);
  }
}
#endif

static void agateDump(AgateVM *vm, const char *context) {
  static const char *AgateCallFrameKindString[] = {
    "EXTERNAL",
    "FOREIGN",
    "INTERNAL",
  };

  printf("--- VM (%s) ---\n", context);

  printf("------ Runtime\n");
  printf("\tstack:           %p\n", (void *) vm->stack);
  printf("\tstack_top:       %p (%td)\n", (void *) vm->stack_top, vm->stack_top - vm->stack);
  printf("\tstack_capacity:  %td\n", vm->stack_capacity);
  printf("\t------ Stack\n\t\t");

  for (AgateValue *ptr = vm->stack; ptr < vm->stack_top; ++ptr) {
    printf("[");
    agateValuePrint(*ptr);
    printf("]");
  }
  printf("\n");

  printf("\tframes:          %p\n", (void *) vm->frames);
  printf("\tframes_count:    %td\n", vm->frames_count);
  printf("\tframes_capacity: %td\n", vm->frames_capacity);

  for (ptrdiff_t i = 0; i < vm->frames_count; ++i) {
    AgateCallFrame *frame = &vm->frames[i];
    printf("\t------ Frame #%td\n", i);
    printf("\t\tkind:        %s\n", AgateCallFrameKindString[frame->kind]);
    printf("\t\tclosure:     %p\n", (void *) frame->closure);
    printf("\t\tip:          %p\n", (void *) frame->ip);
    printf("\t\tstack_start: %p (%td)\n", (void *) frame->stack_start, frame->stack_start - vm->stack);
  }

  printf("--- End ---\n");
}
