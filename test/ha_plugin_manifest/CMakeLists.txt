FetchContent_Declare(
  arduinojson
  GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
  GIT_TAG v7.4.2  # same version as the firmware (platformio.ini)
)
FetchContent_MakeAvailable(arduinojson)

add_executable(HaPluginManifestTest
  HaPluginManifestTest.cpp
)

target_compile_definitions(HaPluginManifestTest PRIVATE REPO_ROOT_DIR="${REPO_ROOT}")

target_link_libraries(HaPluginManifestTest PRIVATE
  crosspoint_test_common
  ArduinoJson
  GTest::gtest_main
)

gtest_discover_tests(HaPluginManifestTest)
