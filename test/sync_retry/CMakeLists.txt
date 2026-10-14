add_executable(SyncRetryTest
  SyncRetryTest.cpp
)

target_link_libraries(SyncRetryTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(SyncRetryTest)
