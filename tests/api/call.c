#include "call.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>

bool agateTestCallRunNative(AgateVM *vm) {
  agateStackStart(vm);
  ptrdiff_t call_slot = agateSlotAllocate(vm);

  assert(agateHasVariable(vm, "tests/api/call.agate", "Call"));
  agateGetVariable(vm, "tests/api/call.agate", "Call", call_slot);
  AgateHandle *call_class_handle = agateSlotGetHandle(vm, call_slot);
  agateStackFinish(vm);

  AgateHandle *no_params_handle = agateMakeCallHandle(vm, "no_params");
  AgateHandle *zero_handle = agateMakeCallHandle(vm, "zero()");
  AgateHandle *one_handle = agateMakeCallHandle(vm, "one(_)");
  AgateHandle *two_handle = agateMakeCallHandle(vm, "two(_,_)");
  AgateHandle *get_value_handle = agateMakeCallHandle(vm, "get_value()");
  AgateHandle *unary_handle = agateMakeCallHandle(vm, "-");
  AgateHandle *binary_handle = agateMakeCallHandle(vm, "-(_)");
  AgateHandle *subscript_getter_handle = agateMakeCallHandle(vm, "[_,_]");
  AgateHandle *subscript_setter_handle = agateMakeCallHandle(vm, "[_,_]=(_)");

  {
    agateStackStart(vm);
    ptrdiff_t arg0 = agateSlotAllocate(vm);
    agateSlotSetHandle(vm, arg0, call_class_handle);
    agateCallHandle(vm, no_params_handle);
    agateStackFinish(vm);
  }

  {
    agateStackStart(vm);
    ptrdiff_t arg0 = agateSlotAllocate(vm);
    agateSlotSetHandle(vm, arg0, call_class_handle);
    agateCallHandle(vm, zero_handle);
    agateStackFinish(vm);
  }

  {
    agateStackStart(vm);
    ptrdiff_t arg0 = agateSlotAllocate(vm);
    agateSlotSetHandle(vm, arg0, call_class_handle);
    ptrdiff_t arg1 = agateSlotAllocate(vm);
    agateSlotSetFloat(vm, arg1, 1.0);
    agateCallHandle(vm, one_handle);
    agateStackFinish(vm);
  }

  {
    agateStackStart(vm);
    ptrdiff_t arg0 = agateSlotAllocate(vm);
    agateSlotSetHandle(vm, arg0, call_class_handle);
    ptrdiff_t arg1 = agateSlotAllocate(vm);
    agateSlotSetFloat(vm, arg1, 1.0);
    ptrdiff_t arg2 = agateSlotAllocate(vm);
    agateSlotSetFloat(vm, arg2, 2.0);
    agateCallHandle(vm, two_handle);
    agateStackFinish(vm);
  }

  {
    agateStackStart(vm);
    ptrdiff_t arg0 = agateSlotAllocate(vm);
    agateSlotSetHandle(vm, arg0, call_class_handle);
    agateCallHandle(vm, unary_handle);
    agateStackFinish(vm);
  }

  {
    agateStackStart(vm);
    ptrdiff_t arg0 = agateSlotAllocate(vm);
    agateSlotSetHandle(vm, arg0, call_class_handle);
    ptrdiff_t arg1 = agateSlotAllocate(vm);
    agateSlotSetFloat(vm, arg1, 1.0);
    agateCallHandle(vm, binary_handle);
    agateStackFinish(vm);
  }

  {
    agateStackStart(vm);
    ptrdiff_t arg0 = agateSlotAllocate(vm);
    agateSlotSetHandle(vm, arg0, call_class_handle);
    ptrdiff_t arg1 = agateSlotAllocate(vm);
    agateSlotSetFloat(vm, arg1, 1.0);
    ptrdiff_t arg2 = agateSlotAllocate(vm);
    agateSlotSetFloat(vm, arg2, 2.0);
    agateCallHandle(vm, subscript_getter_handle);
    agateStackFinish(vm);
  }

  {
    agateStackStart(vm);
    ptrdiff_t arg0 = agateSlotAllocate(vm);
    agateSlotSetHandle(vm, arg0, call_class_handle);
    ptrdiff_t arg1 = agateSlotAllocate(vm);
    agateSlotSetFloat(vm, arg1, 1.0);
    ptrdiff_t arg2 = agateSlotAllocate(vm);
    agateSlotSetFloat(vm, arg2, 2.0);
    ptrdiff_t arg3 = agateSlotAllocate(vm);
    agateSlotSetFloat(vm, arg3, 3.0);
    agateCallHandle(vm, subscript_setter_handle);
    agateStackFinish(vm);
  }

  AgateHandle *handle = NULL;

  {
    agateStackStart(vm);
    ptrdiff_t arg0 = agateSlotAllocate(vm);
    agateSlotSetHandle(vm, arg0, call_class_handle);
    agateCallHandle(vm, get_value_handle);
    printf("slots after call: %td\n", agateSlotCount(vm));
    ptrdiff_t ret = agateSlotForReturn(vm);
    handle = agateSlotGetHandle(vm, ret);
    agateStackFinish(vm);
  }

  {
    agateStackStart(vm);
    ptrdiff_t arg0 = agateSlotAllocate(vm);
    agateSlotSetHandle(vm, arg0, call_class_handle);
    ptrdiff_t arg1 = agateSlotAllocate(vm);
    agateSlotSetBool(vm, arg1, true);
    ptrdiff_t arg2 = agateSlotAllocate(vm);
    agateSlotSetBool(vm, arg2, false);
    agateCallHandle(vm, two_handle);
    agateStackFinish(vm);
  }

  {
    agateStackStart(vm);
    ptrdiff_t arg0 = agateSlotAllocate(vm);
    agateSlotSetHandle(vm, arg0, call_class_handle);
    ptrdiff_t arg1 = agateSlotAllocate(vm);
    agateSlotSetFloat(vm, arg1, 1.2);
    ptrdiff_t arg2 = agateSlotAllocate(vm);
    agateSlotSetFloat(vm, arg2, 3.4);
    agateCallHandle(vm, two_handle);
    agateStackFinish(vm);
  }

  {
    agateStackStart(vm);
    ptrdiff_t arg0 = agateSlotAllocate(vm);
    agateSlotSetHandle(vm, arg0, call_class_handle);
    ptrdiff_t arg1 = agateSlotAllocate(vm);
    agateSlotSetString(vm, arg1, "string");
    ptrdiff_t arg2 = agateSlotAllocate(vm);
    agateSlotSetString(vm, arg2, "another");
    agateCallHandle(vm, two_handle);
    agateStackFinish(vm);
  }

  {
    agateStackStart(vm);
    ptrdiff_t arg0 = agateSlotAllocate(vm);
    agateSlotSetHandle(vm, arg0, call_class_handle);
    ptrdiff_t arg1 = agateSlotAllocate(vm);
    agateSlotSetNil(vm, arg1);
    ptrdiff_t arg2 = agateSlotAllocate(vm);
    agateSlotSetHandle(vm, arg2, handle);
    agateCallHandle(vm, two_handle);
    agateStackFinish(vm);
  }

  {
    agateStackStart(vm);
    ptrdiff_t arg0 = agateSlotAllocate(vm);
    agateSlotSetHandle(vm, arg0, call_class_handle);
    ptrdiff_t arg1 = agateSlotAllocate(vm);
    agateSlotSetStringSize(vm, arg1, "string", 3);
    ptrdiff_t arg2 = agateSlotAllocate(vm);
    agateSlotSetStringSize(vm, arg2, "b\0y\0t\0e", 7);
    agateCallHandle(vm, two_handle);
    agateStackFinish(vm);
  }

//   {
//     agateStackStart(vm);
//
//     ptrdiff_t arg0 = agateSlotAllocate(vm);
//     agateSlotSetHandle(vm, arg0, call_class_handle);
//
//     for (ptrdiff_t i = 1; i < 10; ++i) {
//       ptrdiff_t arg = agateSlotAllocate(vm);
//       agateSlotSetFloat(vm, arg, i * 0.1);
//     }
//
//     agateCallHandle(vm, one_handle);
//     agateStackFinish(vm);
//   }

  agateReleaseHandle(vm, handle);
  agateReleaseHandle(vm, subscript_setter_handle);
  agateReleaseHandle(vm, subscript_getter_handle);
  agateReleaseHandle(vm, binary_handle);
  agateReleaseHandle(vm, unary_handle);
  agateReleaseHandle(vm, get_value_handle);
  agateReleaseHandle(vm, two_handle);
  agateReleaseHandle(vm, one_handle);
  agateReleaseHandle(vm, zero_handle);
  agateReleaseHandle(vm, no_params_handle);
  agateReleaseHandle(vm, call_class_handle);

  return true;
}
