add_library(
	renderer
	STATIC
	src/gl/attribute_formats.cpp
	src/gl/color_formats.cpp
	src/gl/compile_program.cpp
	src/gl/draw.cpp
	src/gl/fence.cpp
	src/gl/renderer.cpp
	src/gl/ring_buffer.cpp
	src/gl/overlay_renderer.cpp
	src/gl/screen_render.cpp
	src/gl/surface_cache.cpp
	src/gl/sync_state.cpp
	src/gl/texture_formats.cpp
	src/gl/texture.cpp
	src/gl/uniforms.cpp

	src/vulkan/allocator.cpp
	src/vulkan/context.cpp
	src/vulkan/creation.cpp
	src/vulkan/gxm_to_vulkan.cpp
	src/vulkan/pipeline_cache.cpp
	src/vulkan/renderer.cpp
	src/vulkan/scene.cpp
	src/vulkan/overlay_renderer.cpp
	src/vulkan/screen_filters.cpp
	src/vulkan/screen_renderer.cpp
	src/vulkan/surface_cache.cpp
	src/vulkan/sync_state.cpp
	src/vulkan/texture.cpp

	src/texture/cache.cpp
	src/texture/format.cpp
	src/texture/palette.cpp
	src/texture/pvrt-dec.cpp
	src/texture/replacement.cpp
	src/texture/yuv.cpp

	src/batch.cpp
	src/creation.cpp
	src/renderer.cpp
	src/scene.cpp
	src/shaders.cpp
	src/state_set.cpp
	src/sync.cpp
	src/transfer.cpp
)

target_include_directories(renderer PUBLIC include)
target_link_libraries(renderer PUBLIC display mem stb shader glutil threads config util vkutil overlay)
target_link_libraries(renderer PRIVATE dialog ddspp SDL3::SDL3 stb ffmpeg xxHash::xxhash concurrentqueue)

if(ANDROID)
	target_link_libraries(renderer PRIVATE android adrenotools)
endif()

if(APPLE)
	set(METAL_SOURCES
		include/renderer/metal/functions.h
		include/renderer/metal/state.h
		include/renderer/metal/types.h
		src/metal/context.mm
		src/metal/gxm_to_metal.h
		src/metal/gxm_to_metal.mm
		src/metal/metal_include.h
		src/metal/mtl_types.h
		src/metal/overlay_renderer.h
		src/metal/overlay_renderer.mm
		src/metal/pipeline_cache.h
		src/metal/pipeline_cache.mm
		src/metal/renderer.mm
		src/metal/scene.mm
		src/metal/screen_renderer.h
		src/metal/screen_renderer.mm
		src/metal/shader_translate.h
		src/metal/shader_translate.mm
		src/metal/surface_cache.h
		src/metal/surface_cache.mm
		src/metal/sync_state.mm
		src/metal/texture.mm)
	target_sources(renderer PRIVATE src/vulkan/metal_layer.mm ${METAL_SOURCES})
	set_source_files_properties(
		src/vulkan/metal_layer.mm
		src/metal/context.mm
		src/metal/gxm_to_metal.mm
		src/metal/overlay_renderer.mm
		src/metal/pipeline_cache.mm
		src/metal/renderer.mm
		src/metal/scene.mm
		src/metal/screen_renderer.mm
		src/metal/shader_translate.mm
		src/metal/surface_cache.mm
		src/metal/sync_state.mm
		src/metal/texture.mm
		PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
	target_link_libraries(renderer PRIVATE spirv-cross-msl spirv-cross-glsl spirv-cross-core "-framework Metal" "-framework QuartzCore" "-framework Cocoa" "-framework Foundation")
endif()

if(X11_FOUND)
	target_link_libraries(renderer PRIVATE ${X11_LIBRARIES})
	target_include_directories(renderer PRIVATE ${X11_INCLUDE_DIR})
endif()

if(WAYLAND_FOUND)
	target_link_libraries(renderer PRIVATE ${WAYLAND_LIBRARIES})
	target_include_directories(renderer PRIVATE ${WAYLAND_INCLUDE_DIRS})
endif()

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
	# silence all vma warnings
	target_compile_options(renderer PRIVATE "-Wno-nullability-completeness")
endif()

# Marshmallow Tracy linking
if(TRACY_ENABLE_ON_CORE_COMPONENTS)
	target_link_libraries(renderer PRIVATE tracy)
endif()
