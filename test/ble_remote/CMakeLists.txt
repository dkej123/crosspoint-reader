add_executable(BleRemoteControllerTest
  BleRemoteControllerTest.cpp
)

target_link_libraries(BleRemoteControllerTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(BleRemoteControllerTest)
