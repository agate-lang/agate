// this file was generated by tests.sh - DO NOT EDIT
static const char *AgateTestFiles[] = {
  "tests/core/array/append.agate",
  "tests/core/array/append_all.agate",
  "tests/core/array/clear.agate",
  "tests/core/array/contains.agate",
  "tests/core/array/count.agate",
  "tests/core/array/count_predicate.agate",
  "tests/core/array/count_predicate_non_bool_returning_fn.agate",
  "tests/core/array/count_predicate_non_function_arg.agate",
  "tests/core/array/each.agate",
  "tests/core/array/each_no_items.agate",
  "tests/core/array/each_non_function_arg.agate",
  "tests/core/array/erase.agate",
  "tests/core/array/erase_index_not_int.agate",
  "tests/core/array/erase_index_not_num.agate",
  "tests/core/array/erase_index_too_large.agate",
  "tests/core/array/erase_index_too_small.agate",
  "tests/core/array/find.agate",
  "tests/core/array/insert.agate",
  "tests/core/array/insert_index_not_int.agate",
  "tests/core/array/insert_index_not_num.agate",
  "tests/core/array/insert_index_too_large.agate",
  "tests/core/array/insert_index_too_small.agate",
  "tests/core/array/iterate.agate",
  "tests/core/array/iterate_iterator_not_int.agate",
  "tests/core/array/iterate_iterator_not_num.agate",
  "tests/core/array/iterator_value.agate",
  "tests/core/array/iterator_value_iterator_not_int.agate",
  "tests/core/array/iterator_value_iterator_not_num.agate",
  "tests/core/array/iterator_value_iterator_too_large.agate",
  "tests/core/array/iterator_value_iterator_too_small.agate",
  "tests/core/array/join.agate",
  "tests/core/array/join_separator_not_string.agate",
  "tests/core/array/map.agate",
  "tests/core/array/multiply.agate",
  "tests/core/array/multiply_negative.agate",
  "tests/core/array/multiply_not_int.agate",
  "tests/core/array/multiply_not_num.agate",
  "tests/core/array/new.agate",
  "tests/core/array/not.agate",
  "tests/core/array/partition.agate",
  "tests/core/array/plus.agate",
  "tests/core/array/plus_not_iterable.agate",
  "tests/core/array/reduce.agate",
  "tests/core/array/reduce_no_items.agate",
  "tests/core/array/reduce_single_item.agate",
  "tests/core/array/reduce_wrong_arity.agate",
  "tests/core/array/remove.agate",
  "tests/core/array/sort.agate",
  "tests/core/array/sort_comp_non_function_arg.agate",
  "tests/core/array/subscript.agate",
  "tests/core/array/subscript_not_int.agate",
  "tests/core/array/subscript_range.agate",
  "tests/core/array/subscript_range_from_not_int.agate",
  "tests/core/array/subscript_range_from_too_large.agate",
  "tests/core/array/subscript_range_from_too_small.agate",
  "tests/core/array/subscript_range_to_exclusive_too_large.agate",
  "tests/core/array/subscript_range_to_exclusive_too_small.agate",
  "tests/core/array/subscript_range_to_not_int.agate",
  "tests/core/array/subscript_range_to_too_large.agate",
  "tests/core/array/subscript_range_to_too_small.agate",
  "tests/core/array/subscript_setter.agate",
  "tests/core/array/subscript_setter_not_int.agate",
  "tests/core/array/subscript_setter_not_num.agate",
  "tests/core/array/subscript_setter_too_large.agate",
  "tests/core/array/subscript_setter_too_small.agate",
  "tests/core/array/subscript_too_large.agate",
  "tests/core/array/subscript_too_small.agate",
  "tests/core/array/subscript_wrong_type.agate",
  "tests/core/array/swap.agate",
  "tests/core/array/swap_out_of_bounds.agate",
  "tests/core/array/to_string.agate",
  "tests/core/array/type.agate",
  "tests/core/array/where.agate",
  "tests/core/bool/equality.agate",
  "tests/core/bool/no_constructor.agate",
  "tests/core/bool/not.agate",
  "tests/core/bool/to_string.agate",
  "tests/core/bool/type.agate",
  "tests/core/class/equality.agate",
  "tests/core/class/name.agate",
  "tests/core/class/no_constructor.agate",
  "tests/core/class/supertype.agate",
  "tests/core/class/type.agate",
  "tests/core/float/comparison.agate",
  "tests/core/float/constants.agate",
  "tests/core/float/decimal_point_at_eof.agate",
  "tests/core/float/divide.agate",
  "tests/core/float/divide_operand_not_num.agate",
  "tests/core/float/equality.agate",
  "tests/core/float/fraction.agate",
  "tests/core/float/greater_than_equal_operand_not_num.agate",
  "tests/core/float/greater_than_operand_not_num.agate",
  "tests/core/float/is_infinity.agate",
  "tests/core/float/is_nan.agate",
  "tests/core/float/less_than_equal_operand_not_num.agate",
  "tests/core/float/less_than_operand_not_num.agate",
  "tests/core/float/minus.agate",
  "tests/core/float/minus_operand_not_num.agate",
  "tests/core/float/multiply.agate",
  "tests/core/float/multiply_operand_not_num.agate",
  "tests/core/float/no_constructor.agate",
  "tests/core/float/not.agate",
  "tests/core/float/plus.agate",
  "tests/core/float/plus_operand_not_num.agate",
  "tests/core/float/to_string.agate",
  "tests/core/float/type.agate",
  "tests/core/function/arity.agate",
  "tests/core/function/call_extra_arguments.agate",
  "tests/core/function/call_missing_arguments.agate",
  "tests/core/function/call_runtime_error.agate",
  "tests/core/function/equality.agate",
  "tests/core/function/new_wrong_arg_type.agate",
  "tests/core/function/to_string.agate",
  "tests/core/function/type.agate",
  "tests/core/int/bitwise_and.agate",
  "tests/core/int/bitwise_and_operand_not_num.agate",
  "tests/core/int/bitwise_lsh.agate",
  "tests/core/int/bitwise_lsh_operand_not_num.agate",
  "tests/core/int/bitwise_not.agate",
  "tests/core/int/bitwise_or.agate",
  "tests/core/int/bitwise_or_operand_not_num.agate",
  "tests/core/int/bitwise_rsh.agate",
  "tests/core/int/bitwise_rsh_operand_not_num.agate",
  "tests/core/int/bitwise_xor.agate",
  "tests/core/int/bitwise_xor_operand_not_num.agate",
  "tests/core/int/comparison.agate",
  "tests/core/int/constants.agate",
  "tests/core/int/divide.agate",
  "tests/core/int/divide_operand_not_num.agate",
  "tests/core/int/equality.agate",
  "tests/core/int/greater_than_equal_operand_not_num.agate",
  "tests/core/int/greater_than_operand_not_num.agate",
  "tests/core/int/invalid_hex_literal.agate",
  "tests/core/int/less_than_equal_operand_not_num.agate",
  "tests/core/int/less_than_operand_not_num.agate",
  "tests/core/int/minus.agate",
  "tests/core/int/minus_operand_not_num.agate",
  "tests/core/int/mod.agate",
  "tests/core/int/mod_operand_not_num.agate",
  "tests/core/int/multiply.agate",
  "tests/core/int/multiply_operand_not_num.agate",
  "tests/core/int/no_constructor.agate",
  "tests/core/int/not.agate",
  "tests/core/int/plus.agate",
  "tests/core/int/plus_operand_not_num.agate",
  "tests/core/int/to_string.agate",
  "tests/core/int/type.agate",
  "tests/core/io/print.agate",
  "tests/core/io/print_all.agate",
  "tests/core/io/print_all_not_sequence.agate",
  "tests/core/io/print_bad_to_string.agate",
  "tests/core/map/churn.agate",
  "tests/core/map/clear.agate",
  "tests/core/map/contains_key.agate",
  "tests/core/map/contains_key_not_value.agate",
  "tests/core/map/count.agate",
  "tests/core/map/empty_string_key.agate",
  "tests/core/map_entry/new.agate",
  "tests/core/map/erase.agate",
  "tests/core/map/erase_key_not_value.agate",
  "tests/core/map/is_empty.agate",
  "tests/core/map/iterate.agate",
  "tests/core/map/iterate_iterator_not_int.agate",
  "tests/core/map/iterate_iterator_not_num.agate",
  "tests/core/map/iterator_value.agate",
  "tests/core/map/iterator_value_iterator_not_int.agate",
  "tests/core/map/iterator_value_iterator_not_num.agate",
  "tests/core/map/iterator_value_iterator_too_large.agate",
  "tests/core/map/iterator_value_iterator_too_small.agate",
  "tests/core/map/key_iterate.agate",
  "tests/core/map/key_iterate_iterator_not_int.agate",
  "tests/core/map/key_iterate_iterator_not_num.agate",
  "tests/core/map/key_types.agate",
  "tests/core/map/new.agate",
  "tests/core/map/reuse_tombstone.agate",
  "tests/core/map/subscript_empty_map.agate",
  "tests/core/map/subscript_key_not_value.agate",
  "tests/core/map/subscript_setter_key_not_value.agate",
  "tests/core/map/to_string.agate",
  "tests/core/map/type.agate",
  "tests/core/map/value_iterate.agate",
  "tests/core/map/value_iterate_iterator_not_int.agate",
  "tests/core/map/value_iterate_iterator_not_num.agate",
  "tests/core/math/abs.agate",
  "tests/core/math/acos.agate",
  "tests/core/math/asin.agate",
  "tests/core/math/atan2.agate",
  "tests/core/math/atan2_x_not_num.agate",
  "tests/core/math/atan.agate",
  "tests/core/math/cbrt.agate",
  "tests/core/math/ceil.agate",
  "tests/core/math/clamp.agate",
  "tests/core/math/clamp_max_not_num.agate",
  "tests/core/math/clamp_min_not_num.agate",
  "tests/core/math/cos.agate",
  "tests/core/math/exp.agate",
  "tests/core/math/floor.agate",
  "tests/core/math/log2.agate",
  "tests/core/math/log.agate",
  "tests/core/math/min_max.agate",
  "tests/core/math/pow.agate",
  "tests/core/math/round.agate",
  "tests/core/math/sin.agate",
  "tests/core/math/sqrt.agate",
  "tests/core/math/tan.agate",
  "tests/core/math/truncate.agate",
  "tests/core/nil/no_constructor.agate",
  "tests/core/nil/not.agate",
  "tests/core/nil/type.agate",
  "tests/core/object/is.agate",
  "tests/core/object/no_constructor.agate",
  "tests/core/object/nonclass_on_right.agate",
  "tests/core/object/not.agate",
  "tests/core/object/same.agate",
  "tests/core/object/to_string.agate",
  "tests/core/object/type.agate",
  "tests/core/range/contains.agate",
  "tests/core/range/equality.agate",
  "tests/core/range/exclusive_range_wrong_rhs_type.agate",
  "tests/core/range/from.agate",
  "tests/core/range/inclusive_range_wrong_rhs_type.agate",
  "tests/core/range/is_inclusive.agate",
  "tests/core/range/iterate.agate",
  "tests/core/range/iterate_wrong_type.agate",
  "tests/core/range/iterator_value.agate",
  "tests/core/range/join.agate",
  "tests/core/range/join_separator_not_string.agate",
  "tests/core/range/map.agate",
  "tests/core/range/max.agate",
  "tests/core/range/min.agate",
  "tests/core/range/no_constructor.agate",
  "tests/core/range/reduce.agate",
  "tests/core/range/to.agate",
  "tests/core/range/to_string.agate",
  "tests/core/range/type.agate",
  "tests/core/range/where.agate",
  "tests/core/sequence/all.agate",
  "tests/core/sequence/all_non_function_arg.agate",
  "tests/core/sequence/any.agate",
  "tests/core/sequence/any_non_function_arg.agate",
  "tests/core/sequence/count.agate",
  "tests/core/sequence/is_empty.agate",
  "tests/core/sequence/map.agate",
  "tests/core/sequence/no_constructor.agate",
  "tests/core/sequence/skip.agate",
  "tests/core/sequence/skip_negative.agate",
  "tests/core/sequence/skip_not_int.agate",
  "tests/core/sequence/skip_not_num.agate",
  "tests/core/sequence/take.agate",
  "tests/core/sequence/take_negative.agate",
  "tests/core/sequence/take_not_int.agate",
  "tests/core/sequence/take_not_num.agate",
  "tests/core/sequence/to_a.agate",
  "tests/core/sequence/where.agate",
  "tests/core/string/concatenation.agate",
  "tests/core/string/concatenation_wrong_arg_type.agate",
  "tests/core/string/contains.agate",
  "tests/core/string/contains_argument_not_string.agate",
  "tests/core/string/count.agate",
  "tests/core/string/ends_with.agate",
  "tests/core/string/ends_with_invalid_arg.agate",
  "tests/core/string/equality.agate",
  "tests/core/string/find.agate",
  "tests/core/string/find_invalid_arg.agate",
  "tests/core/string/find_start.agate",
  "tests/core/string/find_start_not_int.agate",
  "tests/core/string/find_start_not_num.agate",
  "tests/core/string/find_start_too_large.agate",
  "tests/core/string/find_start_too_small.agate",
  "tests/core/string/iterate.agate",
  "tests/core/string/iterate_iterator_not_int.agate",
  "tests/core/string/iterate_iterator_not_num.agate",
  "tests/core/string/iterator_value.agate",
  "tests/core/string/iterator_value_iterator_not_int.agate",
  "tests/core/string/iterator_value_iterator_not_num.agate",
  "tests/core/string/iterator_value_iterator_too_large.agate",
  "tests/core/string/iterator_value_iterator_too_small.agate",
  "tests/core/string/iterator_value_not_valid.agate",
  "tests/core/string/join.agate",
  "tests/core/string/join_separator_not_string.agate",
  "tests/core/string/multiply.agate",
  "tests/core/string/multiply_negative.agate",
  "tests/core/string/multiply_not_int.agate",
  "tests/core/string/multiply_not_num.agate",
  "tests/core/string/no_constructor.agate",
  "tests/core/string/not.agate",
  "tests/core/string/replace.agate",
  "tests/core/string/replace_empty_old.agate",
  "tests/core/string/replace_new_not_string.agate",
  "tests/core/string/replace_old_not_string.agate",
  "tests/core/string/split.agate",
  "tests/core/string/split_argument_not_string.agate",
  "tests/core/string/split_char.agate",
  "tests/core/string/split_empty_seperator.agate",
  "tests/core/string/starts_with.agate",
  "tests/core/string/starts_with_invalid_arg.agate",
  "tests/core/string/to_string.agate",
  "tests/core/string/trim.agate",
  "tests/core/string/trim_chars_not_string.agate",
  "tests/core/string/trim_end.agate",
  "tests/core/string/trim_end_chars_not_string.agate",
  "tests/core/string/trim_start.agate",
  "tests/core/string/trim_start_chars_not_string.agate",
  "tests/core/string/type.agate",
  "tests/docs/array_append.agate",
  "tests/docs/array_append_all.agate",
  "tests/docs/array_clear.agate",
  "tests/docs/array_count.agate",
  "tests/docs/array_erase.agate",
  "tests/docs/array_find.agate",
  "tests/docs/array_insert.agate",
  "tests/docs/array_new.agate",
  "tests/docs/array_partition.agate",
  "tests/docs/array_plus.agate",
  "tests/docs/array_remove.agate",
  "tests/docs/array_sort.agate",
  "tests/docs/array_subscript_getter.agate",
  "tests/docs/array_subscript_setter.agate",
  "tests/docs/array_swap.agate",
  "tests/docs/bool_hash.agate",
  "tests/docs/bool_not.agate",
  "tests/docs/bool_to_i.agate",
  "tests/docs/bool_to_s.agate",
  "tests/docs/char_hash.agate",
  "tests/docs/char_to_i.agate",
  "tests/docs/char_to_s.agate",
  "tests/docs/fn_arity.agate",
  "tests/docs/fn_new.agate",
  "tests/docs/fn_to_s.agate",
  "tests/docs/nil_hash.agate",
  "tests/docs/nil_not.agate",
  "tests/docs/nil_to_s.agate",
  "tests/docs/system_abort.agate",
  "tests/docs/system_clock.agate",
  "tests/docs/system_env.agate",
  "tests/docs/system_gc.agate",
  "tests/language/array/duplicate_comma.agate",
  "tests/language/array/duplicate_trailing_comma.agate",
  "tests/language/array/empty_list_with_comma.agate",
  "tests/language/array/eof_after_comma.agate",
  "tests/language/array/eof_after_element.agate",
  "tests/language/array/grow_shrink.agate",
  "tests/language/array/newline_before_comma.agate",
  "tests/language/array/newlines.agate",
  "tests/language/array/trailing_comma.agate",
  "tests/language/assert/assert.agate",
  "tests/language/assert/no_reason.agate",
  "tests/language/assignment/associativity.agate",
  "tests/language/assignment/global.agate",
  "tests/language/assignment/grouping.agate",
  "tests/language/assignment/infix_operator.agate",
  "tests/language/assignment/is.agate",
  "tests/language/assignment/local.agate",
  "tests/language/assignment/prefix_operator.agate",
  "tests/language/assignment/syntax.agate",
  "tests/language/assignment/undefined.agate",
  "tests/language/bitwise_precedence.agate",
  "tests/language/break/closure_in_for.agate",
  "tests/language/break/closure_in_while.agate",
  "tests/language/break/in_for_loop.agate",
  "tests/language/break/in_function_in_loop.agate",
  "tests/language/break/in_method_in_loop.agate",
  "tests/language/break/in_while_loop.agate",
  "tests/language/break/nested_for_loop.agate",
  "tests/language/break/nested_while_loop.agate",
  "tests/language/break/outside_loop.agate",
  "tests/language/class/field_in_foreign_class.agate",
  "tests/language/class/foreign_class_inherit_fields.agate",
  "tests/language/class/missing_class_after_foreign.agate",
  "tests/language/class/name_inside_body.agate",
  "tests/language/class/newline_after_class.agate",
  "tests/language/class/newline_after_static.agate",
  "tests/language/class/syntax.agate",
  "tests/language/closure/assign_to_closure.agate",
  "tests/language/closure/closed_closure_in_function.agate",
  "tests/language/closure/close_over_function_parameter.agate",
  "tests/language/closure/close_over_later_variable.agate",
  "tests/language/closure/close_over_method_parameter.agate",
  "tests/language/closure/nested_closure.agate",
  "tests/language/closure/open_closure_in_function.agate",
  "tests/language/closure/reference_closure_multiple_times.agate",
  "tests/language/closure/reuse_closure_slot.agate",
  "tests/language/closure/shadow_closure_with_local.agate",
  "tests/language/closure/unused_closure.agate",
  "tests/language/closure/unused_later_closure.agate",
  "tests/language/comments/line_at_eof.agate",
  "tests/language/comments/only_line_comment.agate",
  "tests/language/comments/only_line_comment_and_line.agate",
  "tests/language/comments/unicode.agate",
  "tests/language/conditional/missing_colon.agate",
  "tests/language/conditional/missing_condition.agate",
  "tests/language/conditional/missing_else.agate",
  "tests/language/conditional/missing_question.agate",
  "tests/language/conditional/missing_then.agate",
  "tests/language/conditional/precedence.agate",
  "tests/language/conditional/short_circuit.agate",
  "tests/language/constructor/cannot_be_infix.agate",
  "tests/language/constructor/cannot_be_minus.agate",
  "tests/language/constructor/cannot_be_setter.agate",
  "tests/language/constructor/cannot_be_static.agate",
  "tests/language/constructor/cannot_be_subscript.agate",
  "tests/language/constructor/cannot_be_unary.agate",
  "tests/language/constructor/cannot_call_initializer.agate",
  "tests/language/constructor/cannot_return_value.agate",
  "tests/language/constructor/named.agate",
  "tests/language/constructor/no_default.agate",
  "tests/language/constructor/no_parameter_list.agate",
  "tests/language/constructor/not_inherited.agate",
  "tests/language/constructor/return_without_value.agate",
  "tests/language/constructor/superclass.agate",
  "tests/language/constructor/super_must_have_args.agate",
  "tests/language/continue/closure_in_for.agate",
  "tests/language/continue/closure_in_while.agate",
  "tests/language/continue/in_for_loop.agate",
  "tests/language/continue/in_function_in_loop.agate",
  "tests/language/continue/in_method_in_loop.agate",
  "tests/language/continue/in_while_loop.agate",
  "tests/language/continue/nested_for_loop.agate",
  "tests/language/continue/nested_while_loop.agate",
  "tests/language/continue/outside_loop.agate",
  "tests/language/deeply_nested_gc.agate",
  "tests/language/empty_block.agate",
  "tests/language/empty_file.agate",
  "tests/language/field/closure.agate",
  "tests/language/field/default_to_null.agate",
  "tests/language/field/in_fn_in_static_method.agate",
  "tests/language/field/in_static_method.agate",
  "tests/language/field/in_static_method_in_nested_class.agate",
  "tests/language/field/multiple.agate",
  "tests/language/field/nested_class.agate",
  "tests/language/field/object_reference.agate",
  "tests/language/field/outside_class.agate",
  "tests/language/field/use_before_set.agate",
  "tests/language/for/close_over_loop_variable.agate",
  "tests/language/for/closure_in_body.agate",
  "tests/language/foreign/foreign_after_static.agate",
  "tests/language/foreign/foreign_method_with_body.agate",
  "tests/language/foreign/unknown_method.agate",
  "tests/language/for/newline_after_for.agate",
  "tests/language/for/newline_before_in.agate",
  "tests/language/for/only_evaluate_sequence_once.agate",
  "tests/language/for/return_closure.agate",
  "tests/language/for/return_inside.agate",
  "tests/language/for/syntax.agate",
  "tests/language/for/truth.agate",
  "tests/language/function/empty_body.agate",
  "tests/language/function/newline_body.agate",
  "tests/language/function/newline_in_expression_block.agate",
  "tests/language/function/no_newline_before_close.agate",
  "tests/language/function/no_parameters.agate",
  "tests/language/function/parameters.agate",
  "tests/language/function/recursive.agate",
  "tests/language/function/syntax.agate",
  "tests/language/if/dangling_else.agate",
  "tests/language/if/else.agate",
  "tests/language/if/if.agate",
  "tests/language/if/newline_after_else.agate",
  "tests/language/if/newline_after_if.agate",
  "tests/language/if/truth.agate",
  "tests/language/inheritance/do_not_inherit_static_methods.agate",
  "tests/language/inheritance/inherited_fields_in_closure.agate",
  "tests/language/inheritance/inherit_fields.agate",
  "tests/language/inheritance/inherit_from_array.agate",
  "tests/language/inheritance/inherit_from_bool.agate",
  "tests/language/inheritance/inherit_from_char.agate",
  "tests/language/inheritance/inherit_from_class.agate",
  "tests/language/inheritance/inherit_from_closure.agate",
  "tests/language/inheritance/inherit_from_float.agate",
  "tests/language/inheritance/inherit_from_fn.agate",
  "tests/language/inheritance/inherit_from_int.agate",
  "tests/language/inheritance/inherit_from_map.agate",
  "tests/language/inheritance/inherit_from_nil.agate",
  "tests/language/inheritance/inherit_from_nil_class.agate",
  "tests/language/inheritance/inherit_from_nonclass.agate",
  "tests/language/inheritance/inherit_from_range.agate",
  "tests/language/inheritance/inherit_from_string.agate",
  "tests/language/inheritance/inherit_methods.agate",
  "tests/language/inheritance/is.agate",
  "tests/language/interpolation/empty.agate",
  "tests/language/interpolation/interpolation.agate",
  "tests/language/interpolation/runtime_error_in_expression.agate",
  "tests/language/interpolation/unterminated.agate",
  "tests/language/interpolation/unterminated_expression.agate",
  "tests/language/logical_operator/and.agate",
  "tests/language/logical_operator/and_truth.agate",
  "tests/language/logical_operator/or.agate",
  "tests/language/logical_operator/or_truth.agate",
  "tests/language/many_reallocations.agate",
  "tests/language/map/bad_key_precedence.agate",
  "tests/language/map/duplicate_comma.agate",
  "tests/language/map/duplicate_trailing_comma.agate",
  "tests/language/map/empty_map_with_comma.agate",
  "tests/language/map/eof_after_colon.agate",
  "tests/language/map/eof_after_comma.agate",
  "tests/language/map/eof_after_key.agate",
  "tests/language/map/eof_after_value.agate",
  "tests/language/map/grow_and_shrink.agate",
  "tests/language/map/newlines.agate",
  "tests/language/map/precedence.agate",
  "tests/language/map/trailing_comma.agate",
  "tests/language/method/arity.agate",
  "tests/language/method/call_name_too_long.agate",
  "tests/language/method/duplicate_methods.agate",
  "tests/language/method/duplicate_static_methods.agate",
  "tests/language/method/empty_block.agate",
  "tests/language/method/empty_subscript_call.agate",
  "tests/language/method/empty_subscript_definition.agate",
  "tests/language/method/long_name.agate",
  "tests/language/method/many_methods.agate",
  "tests/language/method/name_too_long.agate",
  "tests/language/method/not_found.agate",
  "tests/language/method/not_found_eleven_arguments.agate",
  "tests/language/method/not_found_multiple_arguments.agate",
  "tests/language/method/not_found_one_argument.agate",
  "tests/language/method/operators.agate",
  "tests/language/method/static.agate",
  "tests/language/method/static_method_not_found.agate",
  "tests/language/method/static_operators.agate",
  "tests/language/method/subscript_operators.agate",
  "tests/language/method/subscript_setter_too_many_arguments.agate",
  "tests/language/method/subscript_too_many_arguments.agate",
  "tests/language/method/too_many_arguments.agate",
  "tests/language/method/too_many_parameters.agate",
  "tests/language/module/change_imported_value/change_imported_value.agate",
  "tests/language/module/compile_error/compile_error.agate",
  "tests/language/module/cyclic_import/cyclic_import.agate",
  "tests/language/module/implicitly_imports_core/implicitly_imports_core.agate",
  "tests/language/module/import_as/import_as.agate",
  "tests/language/module/inside_block/inside_block.agate",
  "tests/language/module/missing_for.agate",
  "tests/language/module/missing_string_after_import.agate",
  "tests/language/module/module_dir/module_dir.agate",
  "tests/language/module/module_dir/something/module.agate",
  "tests/language/module/multiple_variables/multiple_variables.agate",
  "tests/language/module/name_collision.agate",
  "tests/language/module/no_variable/no_variable.agate",
  "tests/language/module/returns/return.agate",
  "tests/language/module/returns/return_from_import.agate",
  "tests/language/module/returns/return_value.agate",
  "tests/language/module/returns/return_value_from_import.agate",
  "tests/language/module/shared_import/shared_import.agate",
  "tests/language/module/simple_import/simple_import.agate",
  "tests/language/module/unknown_module.agate",
  "tests/language/module/unknown_variable/unknown_variable.agate",
  "tests/language/nil/literal.agate",
  "tests/language/nonlocal/assignment.agate",
  "tests/language/nonlocal/duplicate_nonlocal.agate",
  "tests/language/nonlocal/in_block_scope.agate",
  "tests/language/nonlocal/mutual_recursion.agate",
  "tests/language/nonlocal/nonlocal_in_initializer.agate",
  "tests/language/nonlocal/nonlocal_without_initializer.agate",
  "tests/language/nonlocal/null_before_defined.agate",
  "tests/language/nonlocal/undefined.agate",
  "tests/language/nonlocal/use_in_function.agate",
  "tests/language/nonlocal/use_in_function_before_definition.agate",
  "tests/language/nonlocal/use_in_method.agate",
  "tests/language/nonlocal/use_in_method_before_definition.agate",
  "tests/language/number/hex_literals.agate",
  "tests/language/number/hex_too_large.agate",
  "tests/language/number/literals.agate",
  "tests/language/number/literal_too_large.agate",
  "tests/language/number/scientific_floating_exponent.agate",
  "tests/language/number/scientific_float_missing_exponent.agate",
  "tests/language/number/scientific_literals.agate",
  "tests/language/number/scientific_missing_exponent.agate",
  "tests/language/number/scientific_missing_fractional_part.agate",
  "tests/language/number/scientific_multiple_exponants.agate",
  "tests/language/number/scientific_multiple_exponent_signs.agate",
  "tests/language/precedence.agate",
  "tests/language/return/after_else.agate",
  "tests/language/return/after_if.agate",
  "tests/language/return/after_while.agate",
  "tests/language/return/in_function.agate",
  "tests/language/return/in_method.agate",
  "tests/language/return/return_null_if_newline.agate",
  "tests/language/semicolon.agate",
  "tests/language/setter/associativity.agate",
  "tests/language/setter/grouping.agate",
  "tests/language/setter/infix_operator.agate",
  "tests/language/setter/instance.agate",
  "tests/language/setter/is.agate",
  "tests/language/setter/prefix_operator.agate",
  "tests/language/setter/result.agate",
  "tests/language/setter/same_name_as_method.agate",
  "tests/language/setter/static.agate",
  "tests/language/static_field/closure.agate",
  "tests/language/static_field/default_to_null.agate",
  "tests/language/static_field/in_instance_method.agate",
  "tests/language/static_field/multiple.agate",
  "tests/language/static_field/nested_class.agate",
  "tests/language/static_field/outside_class.agate",
  "tests/language/static_field/use_before_set.agate",
  "tests/language/string/escapes.agate",
  "tests/language/string/incomplete_byte_escape.agate",
  "tests/language/string/incomplete_byte_escape_at_eof.agate",
  "tests/language/string/incomplete_long_unicode_escape.agate",
  "tests/language/string/incomplete_unicode_escape.agate",
  "tests/language/string/incomplete_unicode_escape_at_eof.agate",
  "tests/language/string/invalid_byte_escape.agate",
  "tests/language/string/invalid_escape.agate",
  "tests/language/string/invalid_unicode_escape.agate",
  "tests/language/string/literals.agate",
  "tests/language/string/unicode_escapes.agate",
  "tests/language/string/unicode_two_bytes_to_long_escape.agate",
  "tests/language/string/unterminated.agate",
  "tests/language/super/call_different_arity.agate",
  "tests/language/super/call_other_method.agate",
  "tests/language/super/call_same_method.agate",
  "tests/language/super/closure.agate",
  "tests/language/super/implicit_name.agate",
  "tests/language/super/indirectly_inherited.agate",
  "tests/language/super/no_superclass_method.agate",
  "tests/language/super/super_at_top_level.agate",
  "tests/language/super/super_in_closure_in_inherited_method.agate",
  "tests/language/super/super_in_inherited_method.agate",
  "tests/language/super/super_in_static_method.agate",
  "tests/language/super/super_in_top_level_function.agate",
  "tests/language/this/closure.agate",
  "tests/language/this_method/inherited_methods.agate",
  "tests/language/this_method/instance_methods.agate",
  "tests/language/this_method/locals_shadow_getter.agate",
  "tests/language/this_method/locals_shadow_setter.agate",
  "tests/language/this_method/nested_class.agate",
  "tests/language/this_method/static_methods.agate",
  "tests/language/this/nested_class.agate",
  "tests/language/this/nested_closure.agate",
  "tests/language/this/this_at_top_level.agate",
  "tests/language/this/this_in_method.agate",
  "tests/language/this/this_in_static_method.agate",
  "tests/language/this/this_in_top_level_function.agate",
  "tests/language/unexpected_character.agate",
  "tests/language/variable/duplicate_local.agate",
  "tests/language/variable/duplicate_parameter.agate",
  "tests/language/variable/global_in_initializer.agate",
  "tests/language/variable/global_without_initializer.agate",
  "tests/language/variable/local_collide_with_function_parameter.agate",
  "tests/language/variable/local_collide_with_method_parameter.agate",
  "tests/language/variable/local_in_initializer.agate",
  "tests/language/variable/local_in_middle_of_block.agate",
  "tests/language/variable/local_in_nested_block.agate",
  "tests/language/variable/local_outside_method.agate",
  "tests/language/variable/local_without_initializer.agate",
  "tests/language/variable/many_locals.agate",
  "tests/language/variable/many_nonsimultaneous_locals.agate",
  "tests/language/variable/newline_after_def.agate",
  "tests/language/variable/newline_after_equals.agate",
  "tests/language/variable/outside_method.agate",
  "tests/language/variable/scope_if.agate",
  "tests/language/variable/scope_reuse_in_different_blocks.agate",
  "tests/language/variable/scope_while.agate",
  "tests/language/variable/shadow_and_local.agate",
  "tests/language/variable/shadow_global.agate",
  "tests/language/variable/shadow_in_initializer.agate",
  "tests/language/variable/shadow_local.agate",
  "tests/language/variable/too_many_locals.agate",
  "tests/language/variable/too_many_locals_nested.agate",
  "tests/language/variable/undefined_global.agate",
  "tests/language/variable/undefined_local.agate",
  "tests/language/variable/use_false_as_var.agate",
  "tests/language/variable/use_field_as_var.agate",
  "tests/language/variable/use_nil_as_var.agate",
  "tests/language/variable/use_this_as_var.agate",
  "tests/language/variable/use_true_as_var.agate",
  "tests/language/while/closure_in_body.agate",
  "tests/language/while/newline_after_while.agate",
  "tests/language/while/return_closure.agate",
  "tests/language/while/return_inside.agate",
  "tests/language/while/syntax.agate",
  "tests/language/while/truth.agate",
  "tests/language/whitespace.agate",
  "tests/limit/interpolation_nesting.agate",
  "tests/limit/jump_too_far.agate",
  "tests/limit/long_function.agate",
  "tests/limit/long_string.agate",
  "tests/limit/long_variable_name.agate",
  "tests/limit/loop_too_far.agate",
  "tests/limit/many_constants.agate",
  "tests/limit/many_fields.agate",
  "tests/limit/many_globals.agate",
  "tests/limit/many_inherited_fields.agate",
  "tests/limit/reuse_constants.agate",
  "tests/limit/too_many_constants.agate",
  "tests/limit/too_many_fields.agate",
  "tests/limit/too_many_function_parameters.agate",
  "tests/limit/too_many_inherited_fields.agate",
  "tests/limit/too_much_interpolation_nesting.agate",
  "tests/limit/variable_name_too_long.agate",
  NULL
};