# CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

project(project4)

add_executable(mrdemo main.cc)
target_link_libraries(mrdemo mapreducelib)
add_dependencies(mrdemo mapreducelib)

add_executable(mr_worker user_tasks.cc)
target_link_libraries(mr_worker mr_workerlib)
add_dependencies(mr_worker mr_workerlib)

set_target_properties(mrdemo PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/)
set_target_properties(mr_worker PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/)

#config file copy rule
set (source "${CMAKE_CURRENT_SOURCE_DIR}/config.ini")
set (destination "${CMAKE_BINARY_DIR}/bin/config.ini")
add_custom_command(
        TARGET mrdemo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink ${source} ${destination}
        DEPENDS ${destination}
        COMMENT "symbolic link ${destination} created."
)
