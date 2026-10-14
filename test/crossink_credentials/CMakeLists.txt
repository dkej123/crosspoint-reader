add_executable(CrossInkCredentialCompatTest
  CrossInkCredentialCompatTest.cpp
)

target_link_libraries(CrossInkCredentialCompatTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(CrossInkCredentialCompatTest)
