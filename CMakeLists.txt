# CMakeList.txt: line-counter 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required (VERSION 3.10)

# 如果支持，请为 MSVC 编译器启用热重载。
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("line-counter")

# 将源代码添加到此项目的可执行文件。
add_executable (line-counter "main.cpp" "fs.cpp" "fs.hpp" "str.hpp" "utils.hpp" "utils.cpp" "cmdparser.hpp" "cmdparser.cpp")
target_compile_options (line-counter PRIVATE /utf-8)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET line-counter PROPERTY CXX_STANDARD 17)
endif()

# TODO: 如有需要，请添加测试并安装目标。
