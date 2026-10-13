add_executable(AutomaticCheckAbandonTest
  AutomaticCheckAbandonTest.cpp
)

target_include_directories(AutomaticCheckAbandonTest PRIVATE
  ${REPO_ROOT}/lib/KOReaderSync
)

target_link_libraries(AutomaticCheckAbandonTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(AutomaticCheckAbandonTest)
