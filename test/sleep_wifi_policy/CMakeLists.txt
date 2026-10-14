add_executable(SleepWifiPolicyTest
  SleepWifiPolicyTest.cpp
)

target_link_libraries(SleepWifiPolicyTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(SleepWifiPolicyTest)
