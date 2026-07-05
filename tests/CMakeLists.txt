add_executable(dagoba_tests test_dagoba.cpp)
target_link_libraries(dagoba_tests PRIVATE dagoba)
add_test(NAME dagoba_tests COMMAND dagoba_tests)
