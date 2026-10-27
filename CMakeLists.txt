cmake_minimum_required(VERSION 3.22)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# Build main and its declared dependencies; unrelated SDK examples/tests are not application components.
idf_build_set_property(MINIMAL_BUILD ON)
project(embedded_smart_home VERSION 0.2.0)
