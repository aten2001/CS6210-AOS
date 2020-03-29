# CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

project(project3)

add_executable(store store.cc threadpool.h)
target_link_libraries(store 
	Threads::Threads
       	gRPC::grpc++
	p3protolib)
add_dependencies(store p3protolib)
set_target_properties(store PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/)
