project(project3)

add_executable(run_vendors vendor.cc run_vendors.cc)
target_link_libraries(run_vendors 
	Threads::Threads
       	gRPC::grpc++
	p3protolib)
add_dependencies(run_vendors p3protolib)

add_executable(run_tests client.cc run_tests.cc product_queries_util.h)
target_link_libraries(run_tests 
	Threads::Threads
       	gRPC::grpc++
	p3protolib)
add_dependencies(run_tests p3protolib)

set_target_properties(run_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/)
set_target_properties(run_vendors PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/)

