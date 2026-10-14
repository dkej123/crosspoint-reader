add_executable(PluginEventIdTest
  PluginEventIdTest.cpp
)

target_link_libraries(PluginEventIdTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(PluginEventIdTest)
