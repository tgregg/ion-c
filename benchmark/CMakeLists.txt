include_directories(./ inc/)

# C++ standard
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_CXX_STANDARD OR CMAKE_CXX_STANDARD LESS 11)
    set(CMAKE_CXX_STANDARD 11)
endif()


# The public executable.
add_executable(benchmark benchmark.cpp)
#target_include_directories(benchmark PRIVATE ./ ../events ../events/inc docopt)
target_link_libraries(benchmark ionc)
