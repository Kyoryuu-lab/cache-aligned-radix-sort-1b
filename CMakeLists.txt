cmake_minimum_required(VERSION 3.27)
project(Aufgabe_2.2_Optimiert)

find_package(TBB REQUIRED)
set(CMAKE_CXX_STANDARD 20)
add_executable(main main.cpp
        main.cpp)
target_link_libraries(main PRIVATE TBB::tbb)