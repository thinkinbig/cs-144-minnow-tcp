add_library(minnow_testing_debug STATIC common.cc)

add_library(minnow_testing_sanitized EXCLUDE_FROM_ALL STATIC common.cc)
target_compile_options(minnow_testing_sanitized PUBLIC ${SANITIZING_FLAGS})

add_custom_target(functionality_testing)
add_custom_target(speed_testing)

macro(add_test_exec exec_name)
  add_executable("${exec_name}_sanitized" EXCLUDE_FROM_ALL "${exec_name}.cc")
  target_compile_options("${exec_name}_sanitized" PUBLIC ${SANITIZING_FLAGS})
  target_link_options("${exec_name}_sanitized" PUBLIC ${SANITIZING_FLAGS})
  target_link_libraries("${exec_name}_sanitized" minnow_testing_sanitized)
  target_link_libraries("${exec_name}_sanitized" minnow_sanitized)
  target_link_libraries("${exec_name}_sanitized" util_sanitized)
  add_dependencies(functionality_testing "${exec_name}_sanitized")

  add_executable("${exec_name}" EXCLUDE_FROM_ALL "${exec_name}.cc")
  target_link_libraries("${exec_name}" minnow_testing_debug)
  target_link_libraries("${exec_name}" minnow_debug)
  target_link_libraries("${exec_name}" util_debug)
  add_dependencies(functionality_testing "${exec_name}")
endmacro(add_test_exec)

macro(add_speed_test exec_name)
  add_executable("${exec_name}" EXCLUDE_FROM_ALL "${exec_name}.cc")
  target_compile_options("${exec_name}" PUBLIC -O2 -DNDEBUG)
  target_link_libraries("${exec_name}" minnow_optimized)
  target_link_libraries("${exec_name}" util_optimized)
  add_dependencies(speed_testing "${exec_name}")
endmacro(add_speed_test)

add_test_exec(byte_stream_basics)
add_test_exec(byte_stream_capacity)
add_test_exec(byte_stream_one_write)
add_test_exec(byte_stream_two_writes)
add_test_exec(byte_stream_many_writes)
add_test_exec(byte_stream_stress_test)

add_test_exec(reassembler_single)
add_test_exec(reassembler_cap)
add_test_exec(reassembler_seq)
add_test_exec(reassembler_dup)
add_test_exec(reassembler_holes)
add_test_exec(reassembler_overlapping)
add_test_exec(reassembler_win)

add_test_exec(wrapping_integers_cmp)
add_test_exec(wrapping_integers_wrap)
add_test_exec(wrapping_integers_unwrap)
add_test_exec(wrapping_integers_roundtrip)
add_test_exec(wrapping_integers_extra)

add_test_exec(recv_connect)
add_test_exec(recv_transmit)
add_test_exec(recv_window)
add_test_exec(recv_reorder)
add_test_exec(recv_reorder_more)
add_test_exec(recv_close)
add_test_exec(recv_special)

add_test_exec(send_connect)
add_test_exec(send_transmit)
add_test_exec(send_window)
add_test_exec(send_ack)
add_test_exec(send_close)
add_test_exec(send_retx)
add_test_exec(send_extra)

add_test_exec(net_interface)

add_test_exec(router)

add_test_exec(no_skip)

add_test_exec(object_pool_test)

add_speed_test(byte_stream_speed_test)
add_speed_test(reassembler_speed_test)
