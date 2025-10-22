# CMake generated Testfile for 
# Source directory: C:/Users/Chuck/Desktop/CR_AI_Engineering/GameDev/AncestryTree
# Build directory: C:/Users/Chuck/Desktop/CR_AI_Engineering/GameDev/AncestryTree/build_windows
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(ancestrytree_tests "C:/Users/Chuck/Desktop/CR_AI_Engineering/GameDev/AncestryTree/build_windows/bin/ancestrytree_tests.exe")
set_tests_properties(ancestrytree_tests PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/Chuck/Desktop/CR_AI_Engineering/GameDev/AncestryTree/CMakeLists.txt;166;add_test;C:/Users/Chuck/Desktop/CR_AI_Engineering/GameDev/AncestryTree/CMakeLists.txt;0;")
add_test(ancestrytree_memory_check "C:/Program Files (x86)/Dr. Memory/bin/drmemory.exe" "-batch" "-exit_code_if_errors" "1" "C:/Users/Chuck/Desktop/CR_AI_Engineering/GameDev/AncestryTree/build_windows/bin/ancestrytree_tests.exe")
set_tests_properties(ancestrytree_memory_check PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/Chuck/Desktop/CR_AI_Engineering/GameDev/AncestryTree/CMakeLists.txt;187;add_test;C:/Users/Chuck/Desktop/CR_AI_Engineering/GameDev/AncestryTree/CMakeLists.txt;0;")
