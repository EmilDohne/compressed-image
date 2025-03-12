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
