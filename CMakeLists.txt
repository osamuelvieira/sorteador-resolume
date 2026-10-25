cmake_minimum_required(VERSION 3.20)

if(APPLE)
	set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "std::filesystem needs macOS 10.15")
endif()

project(SorteadorResolume VERSION 1.5.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SORTEADOR_BUILD_TESTS "Build the draw logic tests" ON)

set(FFGL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/ffgl)
if(NOT EXISTS ${FFGL_DIR}/source/lib/FFGLSDK.cpp)
	message(FATAL_ERROR "FFGL SDK not found. Run: git submodule update --init")
endif()

# --- web panel embedded as a header ------------------------------------------------------------
set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
add_custom_command(
	OUTPUT ${GENERATED_DIR}/web_assets.h
	COMMAND ${CMAKE_COMMAND}
		-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/web/index.html
		-DOUTPUT=${GENERATED_DIR}/web_assets.h
		-DSYMBOL=kIndexHtml
		-P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedFile.cmake
	DEPENDS web/index.html cmake/EmbedFile.cmake
	COMMENT "Embedding web panel"
)

# --- draw logic, no OpenGL, shared by the plugin and the tests ---------------------------------
add_library(sorteador_core STATIC
	src/Config.cpp
	src/Channel.cpp
)
target_include_directories(sorteador_core PUBLIC src third_party)
set_target_properties(sorteador_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# --- the FFGL plugin ---------------------------------------------------------------------------
add_library(Sorteador MODULE
	src/SorteadorPlugin.cpp
	src/ChannelManager.cpp
	src/TextRenderer.cpp
	src/WebServer.cpp
	src/Platform.cpp
	src/QrCode.cpp
	third_party/qrcodegen/qrcodegen.cpp
	${FFGL_DIR}/source/lib/FFGLSDK.cpp
	${GENERATED_DIR}/web_assets.h
)
target_include_directories(Sorteador PRIVATE
	src
	third_party
	${FFGL_DIR}/source/lib
	${GENERATED_DIR}
)
target_link_libraries(Sorteador PRIVATE sorteador_core)
target_compile_definitions(Sorteador PRIVATE SORTEADOR_VERSION="${PROJECT_VERSION}")

if(MSVC)
	target_compile_options(sorteador_core PUBLIC /utf-8 /W3 /bigobj)
endif()

if(WIN32)
	target_compile_definitions(Sorteador PRIVATE GLEW_STATIC WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
	target_include_directories(Sorteador PRIVATE ${FFGL_DIR}/deps/glew-2.1.0/include)
	target_link_libraries(Sorteador PRIVATE
		${FFGL_DIR}/deps/glew-2.1.0/lib/Release/x64/glew32s.lib
		opengl32
		ws2_32
		shell32
		ole32
	)
	target_sources(Sorteador PRIVATE ${FFGL_DIR}/build/windows/FFGLPlugins.def)
	set_target_properties(Sorteador PROPERTIES SUFFIX ".dll" PREFIX "")
elseif(APPLE)
	find_library(OPENGL_FRAMEWORK OpenGL REQUIRED)
	target_link_libraries(Sorteador PRIVATE ${OPENGL_FRAMEWORK})
	target_compile_definitions(Sorteador PRIVATE GL_SILENCE_DEPRECATION)
	set_target_properties(Sorteador PROPERTIES
		BUNDLE TRUE
		BUNDLE_EXTENSION bundle
		MACOSX_BUNDLE_BUNDLE_NAME Sorteador
		MACOSX_BUNDLE_GUI_IDENTIFIER com.osamuelvieira.sorteador
	)
endif()

find_package(Threads REQUIRED)
target_link_libraries(Sorteador PRIVATE Threads::Threads)

# --- tests -------------------------------------------------------------------------------------
if(SORTEADOR_BUILD_TESTS)
	enable_testing()
	add_executable(sorteador_tests tests/ChannelTest.cpp)
	target_link_libraries(sorteador_tests PRIVATE sorteador_core)
	add_test(NAME sorteador_tests COMMAND sorteador_tests)
endif()

# --- macOS development host: renders the plugin offscreen without Resolume --------------------
if(APPLE AND SORTEADOR_BUILD_TESTS)
	add_executable(sorteador_test_host tools/TestHost.cpp)
	target_include_directories(sorteador_test_host PRIVATE ${FFGL_DIR}/source/lib)
	target_compile_definitions(sorteador_test_host PRIVATE GL_SILENCE_DEPRECATION)
	target_link_libraries(sorteador_test_host PRIVATE ${OPENGL_FRAMEWORK})
	add_dependencies(sorteador_test_host Sorteador)
endif()
