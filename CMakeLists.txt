cmake_minimum_required(VERSION 3.0)
project(timer-display)


find_package(PkgConfig REQUIRED)
pkg_check_modules(WS2811 REQUIRED libws2811)


add_executable(timer-display)
target_sources(timer-display PRIVATE main.cpp res.cpp sync.cpp)
target_link_libraries(timer-display ${WS2811_LIBRARIES} m)
target_include_directories(timer-display PUBLIC ${WS2811_INCLUDE_DIRS})
target_compile_options(timer-display PUBLIC ${WS2811_CFLAGS_OTHER})

