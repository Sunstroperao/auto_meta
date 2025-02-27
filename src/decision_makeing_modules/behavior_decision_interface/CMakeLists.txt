cmake_minimum_required(VERSION 3.10)
project(behavior_decision_interface)

# Default to C99
if(NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 99)
endif()

# Default to C++14
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 14)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(builtin_interfaces REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(std_msgs REQUIRED)
# find_package(behavior_decision_interface)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

set(msg_files
  "msg/FSMDecisionResults.msg"
)

rosidl_generate_interfaces(${PROJECT_NAME} ${msg_files} DEPENDENCIES builtin_interfaces std_msgs ADD_LINTER_TESTS)

ament_export_dependencies(rosidl_default_generators)
ament_export_dependencies(rosidl_default_runtime)
ament_export_include_directories(include)

ament_package()
