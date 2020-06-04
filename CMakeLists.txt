cmake_minimum_required(VERSION 3.12)
project(TransparentWindow)

add_executable(TransparentWindow WIN32)
target_compile_features(TransparentWindow PRIVATE cxx_std_17)
set_target_properties(TransparentWindow PROPERTIES CXX_EXTENSIONS OFF)
target_sources(TransparentWindow PRIVATE TransparentWindow.cpp)
target_link_libraries(TransparentWindow d3d11 dxgi dcomp)
install(TARGETS TransparentWindow DESTINATION .)
