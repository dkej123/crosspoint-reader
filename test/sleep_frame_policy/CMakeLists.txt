add_executable(SleepFramePolicyTest
  SleepFramePolicyTest.cpp
)

target_link_libraries(SleepFramePolicyTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(SleepFramePolicyTest)
