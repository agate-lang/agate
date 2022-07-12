#include "call.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>

bool agateTestCallRunNative(AgateVM *vm) {
  agateEnsureSlots(vm, 1);

  assert(agateHasVariable(vm, "tests/api/call.agate", "Call"));
  agateGetVariable(vm, "tests/api/call.agate", "Call", 0);
  AgateHandle *call_class_handle = agateSlotGetHandle(vm, 0);

  AgateHandle *no_params_handle = agateMakeCallHandle(vm, "no_params");
  AgateHandle *zero_handle = agateMakeCallHandle(vm, "zero()");
  AgateHandle *one_handle = agateMakeCallHandle(vm, "one(_)");
  AgateHandle *two_handle = agateMakeCallHandle(vm, "two(_,_)");
  AgateHandle *get_value_handle = agateMakeCallHandle(vm, "get_value()");
  AgateHandle *unary_handle = agateMakeCallHandle(vm, "-");
  AgateHandle *binary_handle = agateMakeCallHandle(vm, "-(_)");
  AgateHandle *subscript_getter_handle = agateMakeCallHandle(vm, "[_,_]");
  AgateHandle *subscript_setter_handle = agateMakeCallHandle(vm, "[_,_]=(_)");

  agateEnsureSlots(vm, 1);
  agateSlotSetHandle(vm, 0, call_class_handle);
  agateCall(vm, no_params_handle);

  agateEnsureSlots(vm, 1);
  agateSlotSetHandle(vm, 0, call_class_handle);
  agateCall(vm, zero_handle);

  agateEnsureSlots(vm, 2);
  agateSlotSetHandle(vm, 0, call_class_handle);
  agateSlotSetFloat(vm, 1, 1.0);
  agateCall(vm, one_handle);

  agateEnsureSlots(vm, 3);
  agateSlotSetHandle(vm, 0, call_class_handle);
  agateSlotSetFloat(vm, 1, 1.0);
  agateSlotSetFloat(vm, 2, 2.0);
  agateCall(vm, two_handle);

  agateEnsureSlots(vm, 1);
  agateSlotSetHandle(vm, 0, call_class_handle);
  agateCall(vm, unary_handle);

  agateEnsureSlots(vm, 2);
  agateSlotSetHandle(vm, 0, call_class_handle);
  agateSlotSetFloat(vm, 1, 1.0);
  agateCall(vm, binary_handle);

  agateEnsureSlots(vm, 3);
  agateSlotSetHandle(vm, 0, call_class_handle);
  agateSlotSetFloat(vm, 1, 1.0);
  agateSlotSetFloat(vm, 2, 2.0);
  agateCall(vm, subscript_getter_handle);

  agateEnsureSlots(vm, 4);
  agateSlotSetHandle(vm, 0, call_class_handle);
  agateSlotSetFloat(vm, 1, 1.0);
  agateSlotSetFloat(vm, 2, 2.0);
  agateSlotSetFloat(vm, 3, 3.0);
  agateCall(vm, subscript_setter_handle);

  agateEnsureSlots(vm, 1);
  agateSlotSetHandle(vm, 0, call_class_handle);
  agateCall(vm, get_value_handle);
  printf("slots after call: %td\n", agateSlotCount(vm));
  AgateHandle *handle = agateSlotGetHandle(vm, 0);

  agateEnsureSlots(vm, 3);
  agateSlotSetHandle(vm, 0, call_class_handle);
  agateSlotSetBool(vm, 1, true);
  agateSlotSetBool(vm, 2, false);
  agateCall(vm, two_handle);

  agateEnsureSlots(vm, 3);
  agateSlotSetHandle(vm, 0, call_class_handle);
  agateSlotSetFloat(vm, 1, 1.2);
  agateSlotSetFloat(vm, 2, 3.4);
  agateCall(vm, two_handle);

  agateEnsureSlots(vm, 3);
  agateSlotSetHandle(vm, 0, call_class_handle);
  agateSlotSetString(vm, 1, "string");
  agateSlotSetString(vm, 2, "another");
  agateCall(vm, two_handle);

  agateEnsureSlots(vm, 3);
  agateSlotSetHandle(vm, 0, call_class_handle);
  agateSlotSetNil(vm, 1);
  agateSlotSetHandle(vm, 2, handle);
  agateCall(vm, two_handle);

  agateEnsureSlots(vm, 3);
  agateSlotSetHandle(vm, 0, call_class_handle);
  agateSlotSetStringSize(vm, 1, "string", 3);
  agateSlotSetStringSize(vm, 2, "b\0y\0t\0e", 7);
  agateCall(vm, two_handle);

  agateEnsureSlots(vm, 10);
  agateSlotSetHandle(vm, 0, call_class_handle);

  for (int i = 1; i < 10; ++i) {
    agateSlotSetFloat(vm, i, i * 0.1);
  }

  agateCall(vm, one_handle);

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
