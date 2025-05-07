project(CompressedImage)

add_library(compressed_image INTERFACE)
target_include_directories(compressed_image INTERFACE "include")
target_link_libraries(compressed_image INTERFACE  
	blosc2_static 
	blosc2_include 
	nlohmann_json_include 
	OpenImageIO::OpenImageIO
)

if (MSVC)
	target_compile_options(compressed_image INTERFACE /utf-8 /MP /DNOMINMAX)
endif()


# Crank up warning levels on both MSVC, Clang and GCC
if (COMPRESSED_IMAGE_EXTENDED_WARNINGS)
	if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
	target_compile_options(
		compressed_image 
		INTERFACE 
		-Wall 
		-Werror 
		-Wextra
	)
	elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
	target_compile_options(
		compressed_image 
		INTERFACE 
		/W4 
		/WX 
		/w44062 
		/w44464 
		/w45264
	)
	endif()
endif()