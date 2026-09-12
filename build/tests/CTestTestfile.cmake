# CMake generated Testfile for 
# Source directory: /home/fengyue/workspace/pans/tests
# Build directory: /home/fengyue/workspace/pans/build/tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[test_lock]=] "/home/fengyue/workspace/pans/bin/tests/test_lock" "1000")
set_tests_properties([=[test_lock]=] PROPERTIES  TIMEOUT "30" _BACKTRACE_TRIPLES "/home/fengyue/workspace/pans/tests/CMakeLists.txt;38;add_test;/home/fengyue/workspace/pans/tests/CMakeLists.txt;0;")
add_test([=[test_logger]=] "/home/fengyue/workspace/pans/bin/tests/test_logger")
set_tests_properties([=[test_logger]=] PROPERTIES  TIMEOUT "30" _BACKTRACE_TRIPLES "/home/fengyue/workspace/pans/tests/CMakeLists.txt;40;add_test;/home/fengyue/workspace/pans/tests/CMakeLists.txt;0;")
add_test([=[test_mutex]=] "/home/fengyue/workspace/pans/bin/tests/test_mutex" "1000")
set_tests_properties([=[test_mutex]=] PROPERTIES  TIMEOUT "30" _BACKTRACE_TRIPLES "/home/fengyue/workspace/pans/tests/CMakeLists.txt;38;add_test;/home/fengyue/workspace/pans/tests/CMakeLists.txt;0;")
subdirs("inner_tests")
