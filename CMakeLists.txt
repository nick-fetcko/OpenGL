cmake_minimum_required(VERSION 3.9.0 FATAL_ERROR)
project(OpenGL VERSION 0.0.1 LANGUAGES CXX)

include(GNUInstallDirs)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})

add_subdirectory(MathCPP)

set(_glad_dir
	third_party/glad)

set(_lodepng_dir
	third_party/lodepng)

set(_glad_headers
	${_glad_dir}/glad/glad.h
	${_glad_dir}/KHR/khrplatform.h
	)
set(_glad_sources
	${_glad_dir}/glad/glad.c
	)

set(_lodepng_headers
	${_lodepng_dir}/lodepng.h
)

set(_lodepng_sources
	${_lodepng_dir}/lodepng.cpp
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/third_party FILES
	${_glad_headers}
	${_lodepng_headers}
	${_glad_sources}
	${_lodepng_sources}
	)

find_package(glm CONFIG REQUIRED)

add_library(OpenGL STATIC ${_glad_sources} ${_lodepng_sources} Buffer.hpp Context.hpp Framebuffer.hpp OpenGLFont.cpp OpenGLFont.hpp OpenGLVector.cpp OpenGLVector.hpp Polyline.hpp Shader.hpp ShaderProgram.hpp ShaderProgram.cpp Size.hpp Texture.hpp VertexArray.hpp)
set_target_properties(OpenGL PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories(OpenGL PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> ${_glad_dir} ${_lodepng_dir})
target_compile_features(OpenGL PUBLIC cxx_std_17)
target_compile_definitions(OpenGL PUBLIC _CRT_SECURE_NO_WARNINGS)
target_link_libraries(OpenGL PUBLIC Utils MathsCPP glm::glm)