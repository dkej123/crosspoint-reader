add_executable(OrientationRotationTest
  OrientationRotationTest.cpp
)

target_link_libraries(OrientationRotationTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(OrientationRotationTest)
