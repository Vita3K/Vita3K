// dear imgui: Renderer Backend for Vulkan
// This needs to be used along with a Platform Backend (e.g. GLFW, SDL, Win32, custom..)

// Implemented features:
//  [X] Renderer: Support for large meshes (64k+ vertices) with 16-bit indices.
//  [!] Renderer: User texture binding. Use 'VkDescriptorSet' as ImTextureID. Read the FAQ about ImTextureID! See https://github.com/ocornut/imgui/pull/914 for discussions.

// Important: on 32-bit systems, user texture binding is only supported if your imconfig file has '#define ImTextureID ImU64'.
// This is because we need ImTextureID to carry a 64-bit value and by default ImTextureID is defined as void*.
// To build this on 32-bit systems and support texture changes:
// - [Solution 1] IDE/msbuild: in "Properties/C++/Preprocessor Definitions" add 'ImTextureID=ImU64' (this is what we do in our .vcxproj files)
// - [Solution 2] IDE/msbuild: in "Properties/C++/Preprocessor Definitions" add 'IMGUI_USER_CONFIG="my_imgui_config.h"' and inside 'my_imgui_config.h' add '#define ImTextureID ImU64' and as many other options as you like.
// - [Solution 3] IDE/msbuild: edit imconfig.h and add '#define ImTextureID ImU64' (prefer solution 2 to create your own config file!)
// - [Solution 4] command-line: add '/D ImTextureID=ImU64' to your cl.exe command-line (this is what we do in our batch files)

// You can use unmodified imgui_impl_* files in your project. See examples/ folder for examples of using this.
// Prefer including the entire imgui/ repository into your project (either as a copy or as a submodule), and only build the backends you need.
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

// The aim of imgui_impl_vulkan.h/.cpp is to be usable in your engine without any modification.
// IF YOU FEEL YOU NEED TO MAKE ANY CHANGE TO THIS CODE, please share them and your feedback at https://github.com/ocornut/imgui/

// Important note to the reader who wish to integrate imgui_impl_vulkan.cpp/.h in their own engine/app.
// - Common ImGui_ImplVulkan_XXX functions and structures are used to interface with imgui_impl_vulkan.cpp/.h.
//   You will use those if you want to use this rendering backend in your engine/app.
// - Helper ImGui_ImplVulkanH_XXX functions and structures are only used by this example (main.cpp) and by
//   the backend itself (imgui_impl_vulkan.cpp), but should PROBABLY NOT be used by your own engine/app code.
// Read comments in imgui_impl_vulkan.h.

// CHANGELOG
// (minor and older changes stripped away, please see git history for details)
//  2021-10-15: Vulkan: Call vkCmdSetScissor() at the end of render a full-viewport to reduce likehood of issues with people using VK_DYNAMIC_STATE_SCISSOR in their app without calling vkCmdSetScissor() explicitly every frame.
//  2021-06-29: Reorganized backend to pull data from a single structure to facilitate usage with multiple-contexts (all g_XXXX access changed to bd->XXXX).
//  2021-03-22: Vulkan: Fix mapped memory validation error when buffer sizes are not multiple of VkPhysicalDeviceLimits::nonCoherentAtomSize.
//  2021-02-18: Vulkan: Change blending equation to preserve alpha in output buffer.
//  2021-01-27: Vulkan: Added support for custom function load and IMGUI_IMPL_VULKAN_NO_PROTOTYPES by using ImGui_ImplVulkan_LoadFunctions().
//  2020-11-11: Vulkan: Added support for specifying which subpass to reference during VkPipeline creation.
//  2020-09-07: Vulkan: Added VkPipeline parameter to ImGui_ImplVulkan_RenderDrawData (default to one passed to ImGui_ImplVulkan_Init).
//  2020-05-04: Vulkan: Fixed crash if initial frame has no vertices.
//  2020-04-26: Vulkan: Fixed edge case where render callbacks wouldn't be called if the ImDrawData didn't have vertices.
//  2019-08-01: Vulkan: Added support for specifying multisample count. Set ImGui_ImplVulkan_InitInfo::MSAASamples to one of the VkSampleCountFlagBits values to use, default is non-multisampled as before.
//  2019-05-29: Vulkan: Added support for large mesh (64K+ vertices), enable ImGuiBackendFlags_RendererHasVtxOffset flag.
//  2019-04-30: Vulkan: Added support for special ImDrawCallback_ResetRenderState callback to reset render state.
//  2019-04-04: *BREAKING CHANGE*: Vulkan: Added ImageCount/MinImageCount fields in ImGui_ImplVulkan_InitInfo, required for initialization (was previously a hard #define IMGUI_VK_QUEUED_FRAMES 2). Added ImGui_ImplVulkan_SetMinImageCount().
//  2019-04-04: Vulkan: Added VkInstance argument to ImGui_ImplVulkanH_CreateWindow() optional helper.
//  2019-04-04: Vulkan: Avoid passing negative coordinates to vkCmdSetScissor, which debug validation layers do not like.
//  2019-04-01: Vulkan: Support for 32-bit index buffer (#define ImDrawIdx unsigned int).
//  2019-02-16: Vulkan: Viewport and clipping rectangles correctly using draw_data->FramebufferScale to allow retina display.
//  2018-11-30: Misc: Setting up io.BackendRendererName so it can be displayed in the About Window.
//  2018-08-25: Vulkan: Fixed mishandled VkSurfaceCapabilitiesKHR::maxImageCount=0 case.
//  2018-06-22: Inverted the parameters to ImGui_ImplVulkan_RenderDrawData() to be consistent with other backends.
//  2018-06-08: Misc: Extracted imgui_impl_vulkan.cpp/.h away from the old combined GLFW+Vulkan example.
//  2018-06-08: Vulkan: Use draw_data->DisplayPos and draw_data->DisplaySize to setup projection matrix and clipping rectangle.
//  2018-03-03: Vulkan: Various refactor, created a couple of ImGui_ImplVulkanH_XXX helper that the example can use and that viewport support will use.
//  2018-03-01: Vulkan: Renamed ImGui_ImplVulkan_Init_Info to ImGui_ImplVulkan_InitInfo and fields to match more closely Vulkan terminology.
//  2018-02-16: Misc: Obsoleted the io.RenderDrawListsFn callback, ImGui_ImplVulkan_Render() calls ImGui_ImplVulkan_RenderDrawData() itself.
//  2018-02-06: Misc: Removed call to ImGui::Shutdown() which is not available from 1.60 WIP, user needs to call CreateContext/DestroyContext themselves.
//  2017-05-15: Vulkan: Fix scissor offset being negative. Fix new Vulkan validation warnings. Set required depth member for buffer image copy.
//  2016-11-13: Vulkan: Fix validation layer warnings and errors and redeclare gl_PerVertex.
//  2016-10-18: Vulkan: Add location decorators & change to use structs as in/out in glsl, update embedded spv (produced with glslangValidator -x). Null the released resources.
//  2016-08-27: Vulkan: Fix Vulkan example for use when a depth buffer is active.

#include <gui/imgui_impl_sdl_vulkan.h>

#include <renderer/types.h>
#include <renderer/vulkan/state.h>
#include <vkutil/vkutil.h>

#include <util/log.h>

// Visual Studio warnings
#ifdef _MSC_VER
#pragma warning(disable : 4127) // condition expression is constant
#endif

//-----------------------------------------------------------------------------
// SHADERS
//-----------------------------------------------------------------------------

// glsl_shader.vert, compiled with:
// # glslangValidator -V -x -o glsl_shader.vert.u32 glsl_shader.vert
/*
#version 450 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;
layout(push_constant) uniform uPushConstant { vec2 uScale; vec2 uTranslate; } pc;
out gl_PerVertex { vec4 gl_Position; };
layout(location = 0) out struct { vec4 Color; vec2 UV; } Out;
void main()
{
    Out.Color = aColor;
    Out.UV = aUV;
    gl_Position = vec4(aPos * pc.uScale + pc.uTranslate, 0, 1);
}
*/
static uint32_t __glsl_shader_vert_spv[] = {
    0x07230203, 0x00010000, 0x00080001, 0x0000002e, 0x00000000, 0x00020011, 0x00000001, 0x0006000b,
    0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001,
    0x000a000f, 0x00000000, 0x00000004, 0x6e69616d, 0x00000000, 0x0000000b, 0x0000000f, 0x00000015,
    0x0000001b, 0x0000001c, 0x00030003, 0x00000002, 0x000001c2, 0x00040005, 0x00000004, 0x6e69616d,
    0x00000000, 0x00030005, 0x00000009, 0x00000000, 0x00050006, 0x00000009, 0x00000000, 0x6f6c6f43,
    0x00000072, 0x00040006, 0x00000009, 0x00000001, 0x00005655, 0x00030005, 0x0000000b, 0x0074754f,
    0x00040005, 0x0000000f, 0x6c6f4361, 0x0000726f, 0x00030005, 0x00000015, 0x00565561, 0x00060005,
    0x00000019, 0x505f6c67, 0x65567265, 0x78657472, 0x00000000, 0x00060006, 0x00000019, 0x00000000,
    0x505f6c67, 0x7469736f, 0x006e6f69, 0x00030005, 0x0000001b, 0x00000000, 0x00040005, 0x0000001c,
    0x736f5061, 0x00000000, 0x00060005, 0x0000001e, 0x73755075, 0x6e6f4368, 0x6e617473, 0x00000074,
    0x00050006, 0x0000001e, 0x00000000, 0x61635375, 0x0000656c, 0x00060006, 0x0000001e, 0x00000001,
    0x61725475, 0x616c736e, 0x00006574, 0x00030005, 0x00000020, 0x00006370, 0x00040047, 0x0000000b,
    0x0000001e, 0x00000000, 0x00040047, 0x0000000f, 0x0000001e, 0x00000002, 0x00040047, 0x00000015,
    0x0000001e, 0x00000001, 0x00050048, 0x00000019, 0x00000000, 0x0000000b, 0x00000000, 0x00030047,
    0x00000019, 0x00000002, 0x00040047, 0x0000001c, 0x0000001e, 0x00000000, 0x00050048, 0x0000001e,
    0x00000000, 0x00000023, 0x00000000, 0x00050048, 0x0000001e, 0x00000001, 0x00000023, 0x00000008,
    0x00030047, 0x0000001e, 0x00000002, 0x00020013, 0x00000002, 0x00030021, 0x00000003, 0x00000002,
    0x00030016, 0x00000006, 0x00000020, 0x00040017, 0x00000007, 0x00000006, 0x00000004, 0x00040017,
    0x00000008, 0x00000006, 0x00000002, 0x0004001e, 0x00000009, 0x00000007, 0x00000008, 0x00040020,
    0x0000000a, 0x00000003, 0x00000009, 0x0004003b, 0x0000000a, 0x0000000b, 0x00000003, 0x00040015,
    0x0000000c, 0x00000020, 0x00000001, 0x0004002b, 0x0000000c, 0x0000000d, 0x00000000, 0x00040020,
    0x0000000e, 0x00000001, 0x00000007, 0x0004003b, 0x0000000e, 0x0000000f, 0x00000001, 0x00040020,
    0x00000011, 0x00000003, 0x00000007, 0x0004002b, 0x0000000c, 0x00000013, 0x00000001, 0x00040020,
    0x00000014, 0x00000001, 0x00000008, 0x0004003b, 0x00000014, 0x00000015, 0x00000001, 0x00040020,
    0x00000017, 0x00000003, 0x00000008, 0x0003001e, 0x00000019, 0x00000007, 0x00040020, 0x0000001a,
    0x00000003, 0x00000019, 0x0004003b, 0x0000001a, 0x0000001b, 0x00000003, 0x0004003b, 0x00000014,
    0x0000001c, 0x00000001, 0x0004001e, 0x0000001e, 0x00000008, 0x00000008, 0x00040020, 0x0000001f,
    0x00000009, 0x0000001e, 0x0004003b, 0x0000001f, 0x00000020, 0x00000009, 0x00040020, 0x00000021,
    0x00000009, 0x00000008, 0x0004002b, 0x00000006, 0x00000028, 0x00000000, 0x0004002b, 0x00000006,
    0x00000029, 0x3f800000, 0x00050036, 0x00000002, 0x00000004, 0x00000000, 0x00000003, 0x000200f8,
    0x00000005, 0x0004003d, 0x00000007, 0x00000010, 0x0000000f, 0x00050041, 0x00000011, 0x00000012,
    0x0000000b, 0x0000000d, 0x0003003e, 0x00000012, 0x00000010, 0x0004003d, 0x00000008, 0x00000016,
    0x00000015, 0x00050041, 0x00000017, 0x00000018, 0x0000000b, 0x00000013, 0x0003003e, 0x00000018,
    0x00000016, 0x0004003d, 0x00000008, 0x0000001d, 0x0000001c, 0x00050041, 0x00000021, 0x00000022,
    0x00000020, 0x0000000d, 0x0004003d, 0x00000008, 0x00000023, 0x00000022, 0x00050085, 0x00000008,
    0x00000024, 0x0000001d, 0x00000023, 0x00050041, 0x00000021, 0x00000025, 0x00000020, 0x00000013,
    0x0004003d, 0x00000008, 0x00000026, 0x00000025, 0x00050081, 0x00000008, 0x00000027, 0x00000024,
    0x00000026, 0x00050051, 0x00000006, 0x0000002a, 0x00000027, 0x00000000, 0x00050051, 0x00000006,
    0x0000002b, 0x00000027, 0x00000001, 0x00070050, 0x00000007, 0x0000002c, 0x0000002a, 0x0000002b,
    0x00000028, 0x00000029, 0x00050041, 0x00000011, 0x0000002d, 0x0000001b, 0x0000000d, 0x0003003e,
    0x0000002d, 0x0000002c, 0x000100fd, 0x00010038
};

// glsl_shader.frag, compiled with:
// # glslangValidator -V -x -o glsl_shader.frag.u32 glsl_shader.frag
/*
#version 450 core
layout(location = 0) out vec4 fColor;
layout(set=0, binding=0) uniform sampler2D sTexture;
layout(location = 0) in struct { vec4 Color; vec2 UV; } In;
void main()
{
    fColor = In.Color * texture(sTexture, In.UV.st);
}
*/
static uint32_t __glsl_shader_frag_spv[] = {
    0x07230203, 0x00010000, 0x00080001, 0x0000001e, 0x00000000, 0x00020011, 0x00000001, 0x0006000b,
    0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001,
    0x0007000f, 0x00000004, 0x00000004, 0x6e69616d, 0x00000000, 0x00000009, 0x0000000d, 0x00030010,
    0x00000004, 0x00000007, 0x00030003, 0x00000002, 0x000001c2, 0x00040005, 0x00000004, 0x6e69616d,
    0x00000000, 0x00040005, 0x00000009, 0x6c6f4366, 0x0000726f, 0x00030005, 0x0000000b, 0x00000000,
    0x00050006, 0x0000000b, 0x00000000, 0x6f6c6f43, 0x00000072, 0x00040006, 0x0000000b, 0x00000001,
    0x00005655, 0x00030005, 0x0000000d, 0x00006e49, 0x00050005, 0x00000016, 0x78655473, 0x65727574,
    0x00000000, 0x00040047, 0x00000009, 0x0000001e, 0x00000000, 0x00040047, 0x0000000d, 0x0000001e,
    0x00000000, 0x00040047, 0x00000016, 0x00000022, 0x00000000, 0x00040047, 0x00000016, 0x00000021,
    0x00000000, 0x00020013, 0x00000002, 0x00030021, 0x00000003, 0x00000002, 0x00030016, 0x00000006,
    0x00000020, 0x00040017, 0x00000007, 0x00000006, 0x00000004, 0x00040020, 0x00000008, 0x00000003,
    0x00000007, 0x0004003b, 0x00000008, 0x00000009, 0x00000003, 0x00040017, 0x0000000a, 0x00000006,
    0x00000002, 0x0004001e, 0x0000000b, 0x00000007, 0x0000000a, 0x00040020, 0x0000000c, 0x00000001,
    0x0000000b, 0x0004003b, 0x0000000c, 0x0000000d, 0x00000001, 0x00040015, 0x0000000e, 0x00000020,
    0x00000001, 0x0004002b, 0x0000000e, 0x0000000f, 0x00000000, 0x00040020, 0x00000010, 0x00000001,
    0x00000007, 0x00090019, 0x00000013, 0x00000006, 0x00000001, 0x00000000, 0x00000000, 0x00000000,
    0x00000001, 0x00000000, 0x0003001b, 0x00000014, 0x00000013, 0x00040020, 0x00000015, 0x00000000,
    0x00000014, 0x0004003b, 0x00000015, 0x00000016, 0x00000000, 0x0004002b, 0x0000000e, 0x00000018,
    0x00000001, 0x00040020, 0x00000019, 0x00000001, 0x0000000a, 0x00050036, 0x00000002, 0x00000004,
    0x00000000, 0x00000003, 0x000200f8, 0x00000005, 0x00050041, 0x00000010, 0x00000011, 0x0000000d,
    0x0000000f, 0x0004003d, 0x00000007, 0x00000012, 0x00000011, 0x0004003d, 0x00000014, 0x00000017,
    0x00000016, 0x00050041, 0x00000019, 0x0000001a, 0x0000000d, 0x00000018, 0x0004003d, 0x0000000a,
    0x0000001b, 0x0000001a, 0x00050057, 0x00000007, 0x0000001c, 0x00000017, 0x0000001b, 0x00050085,
    0x00000007, 0x0000001d, 0x00000012, 0x0000001c, 0x0003003e, 0x00000009, 0x0000001d, 0x000100fd,
    0x00010038
};

//-----------------------------------------------------------------------------
// FUNCTIONS
//-----------------------------------------------------------------------------

inline static renderer::vulkan::VKState &get_renderer(ImGui_VulkanState &state) {
    return dynamic_cast<renderer::vulkan::VKState &>(*state.renderer);
}

static void TextureUpdateDescriptorSet(ImGui_VulkanState &state, TextureState *texture) {
    auto &vk_state = get_renderer(state);
    // always has the same descriptor set
    if (texture == state.Font)
        return;

    if (texture->last_frame_used == state.frame_timestamp)
        return;
    texture->last_frame_used = state.frame_timestamp;

    texture->descriptor_set = state.DescriptorSets[state.DescriptorIdx];
    state.DescriptorIdx = (state.DescriptorIdx + 1) % TextureState::nb_descriptor_sets;

    // update descriptor set
    vk::DescriptorImageInfo descr_image_info{
        .imageView = texture->image_view,
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
    };
    vk::WriteDescriptorSet write_descr{
        .dstSet = texture->descriptor_set,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
    };
    write_descr.setImageInfo(descr_image_info);
    vk_state.device.updateDescriptorSets(write_descr, {});
}

static void CreateOrResizeBuffer(ImGui_VulkanState &state, vk::Buffer &buffer, vma::Allocation &buffer_allocation, vk::DeviceSize &p_buffer_size, size_t new_size, vk::BufferUsageFlagBits usage) {
    auto &vk_state = get_renderer(state);
    if (buffer)
        vk_state.allocator.destroyBuffer(buffer, buffer_allocation);

    vk::DeviceSize vertex_buffer_size_aligned = ((new_size - 1) / state.BufferMemoryAlignment + 1) * state.BufferMemoryAlignment;
    vk::BufferCreateInfo buffer_info{
        .size = vertex_buffer_size_aligned,
        .usage = usage,
        .sharingMode = vk::SharingMode::eExclusive
    };
    vma::AllocationCreateInfo alloc_info{
        .flags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite,
        .usage = vma::MemoryUsage::eAuto
    };
    std::tie(buffer, buffer_allocation) = vk_state.allocator.createBuffer(buffer_info, alloc_info);
    p_buffer_size = vertex_buffer_size_aligned;
}

static void ImGui_ImplVulkan_SetupRenderState(ImGui_VulkanState &state, ImDrawData *draw_data, vk::Pipeline pipeline, vk::CommandBuffer command_buffer, ImGui_ImplVulkanH_FrameRenderBuffers *rb, int fb_width, int fb_height) {
    // Bind pipeline:
    command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);

    // Bind Vertex And Index Buffer:
    if (draw_data->TotalVtxCount > 0) {
        vk::DeviceSize vertex_offset = 0;
        command_buffer.bindVertexBuffers(0, rb->VertexBuffer, vertex_offset);
        command_buffer.bindIndexBuffer(rb->IndexBuffer, 0, sizeof(ImDrawIdx) == 2 ? vk::IndexType::eUint16 : vk::IndexType::eUint32);
    }

    // Setup viewport:
    {
        vk::Viewport viewport{
            .x = 0,
            .y = 0,
            .width = static_cast<float>(fb_width),
            .height = static_cast<float>(fb_height),
            .minDepth = 0.f,
            .maxDepth = 1.f
        };
        command_buffer.setViewport(0, viewport);
    }

    // Setup scale and translation:
    // Our visible imgui space lies from draw_data->DisplayPps (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayPos is (0,0) for single viewport apps.
    {
        float scale[2];
        scale[0] = 2.0f / draw_data->DisplaySize.x;
        scale[1] = 2.0f / draw_data->DisplaySize.y;
        float translate[2];
        translate[0] = -1.0f - draw_data->DisplayPos.x * scale[0];
        translate[1] = -1.0f - draw_data->DisplayPos.y * scale[1];
        command_buffer.pushConstants(state.PipelineLayout, vk::ShaderStageFlagBits::eVertex, sizeof(float) * 0, sizeof(float) * 2, scale);
        command_buffer.pushConstants(state.PipelineLayout, vk::ShaderStageFlagBits::eVertex, sizeof(float) * 2, sizeof(float) * 2, translate);
    }
}

// constexpr vk::IndexType imgui_index_type = sizeof(ImDrawIdx) == 2 ? vk::IndexType::eUint16 : vk::IndexType::eUint32;

IMGUI_API ImGui_VulkanState *ImGui_ImplSdlVulkan_Init(renderer::State *renderer, SDL_Window *window) {
    auto *state = new ImGui_VulkanState;
    state->renderer = renderer;
    state->window = window;
    auto &vk_state = get_renderer(*state);

    vk::ShaderModuleCreateInfo vert_info{
        .codeSize = sizeof(__glsl_shader_vert_spv),
        .pCode = reinterpret_cast<uint32_t *>(__glsl_shader_vert_spv)
    };
    state->ShaderModuleVert = vk_state.device.createShaderModule(vert_info);

    vk::ShaderModuleCreateInfo frag_info{
        .codeSize = sizeof(__glsl_shader_frag_spv),
        .pCode = reinterpret_cast<uint32_t *>(__glsl_shader_frag_spv)
    };
    state->ShaderModuleFrag = vk_state.device.createShaderModule(frag_info);

    return state;
}

IMGUI_API void ImGui_ImplSdlVulkan_Shutdown(ImGui_VulkanState &state) {
    ImGui_ImplSdlVulkan_InvalidateDeviceObjects(state);

    get_renderer(state).device.destroy(state.ShaderModuleVert);
    get_renderer(state).device.destroy(state.ShaderModuleFrag);
}

static void ImGui_ImplSdlVulkan_DeletePipeline(ImGui_VulkanState &state) {
    get_renderer(state).device.destroy(state.Pipeline);
}

static bool ImGui_ImplSdlVulkan_CreatePipeline(ImGui_VulkanState &state) {
    auto &vk_state = get_renderer(state);

    std::vector<vk::PipelineShaderStageCreateInfo> shader_stage_infos = {
        vk::PipelineShaderStageCreateInfo{
            .stage = vk::ShaderStageFlagBits::eVertex,
            .module = state.ShaderModuleVert,
            .pName = "main" },
        vk::PipelineShaderStageCreateInfo{
            .stage = vk::ShaderStageFlagBits::eFragment,
            .module = state.ShaderModuleFrag,
            .pName = "main" },
    };

    std::vector<vk::VertexInputBindingDescription> gui_pipeline_bindings = {
        vk::VertexInputBindingDescription{
            .binding = 0,
            .stride = sizeof(ImDrawVert) },
    };

    std::vector<vk::VertexInputAttributeDescription> gui_pipeline_attributes = {
        vk::VertexInputAttributeDescription{
            .location = 0,
            .binding = 0,
            .format = vk::Format::eR32G32Sfloat,
            .offset = 0 },
        vk::VertexInputAttributeDescription{
            .location = 1,
            .binding = 0,
            .format = vk::Format::eR32G32Sfloat,
            .offset = sizeof(ImVec2) },
        vk::VertexInputAttributeDescription{
            .location = 2,
            .binding = 0,
            .format = vk::Format::eR8G8B8A8Unorm,
            .offset = sizeof(ImVec2) * 2 },
    };

    vk::PipelineVertexInputStateCreateInfo gui_pipeline_vertex_info{
        .vertexBindingDescriptionCount = static_cast<uint32_t>(gui_pipeline_bindings.size()),
        .pVertexBindingDescriptions = gui_pipeline_bindings.data(), // Bindings
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(gui_pipeline_attributes.size()),
        .pVertexAttributeDescriptions = gui_pipeline_attributes.data() // Attributes
    };

    vk::PipelineInputAssemblyStateCreateInfo gui_pipeline_assembly_info{
        .topology = vk::PrimitiveTopology::eTriangleList,
        .primitiveRestartEnable = false
    };

    vk::PipelineViewportStateCreateInfo gui_pipeline_viewport_info{
        .viewportCount = 1,
        .scissorCount = 1,
    };

    vk::PipelineRasterizationStateCreateInfo gui_pipeline_rasterization_info{
        .polygonMode = vk::PolygonMode::eFill, // Fill Polygons
        .cullMode = vk::CullModeFlagBits::eNone,
        .frontFace = vk::FrontFace::eCounterClockwise, // Counter Clockwise Face Forwards
        .lineWidth = 1.0f // Line Width
    };

    vk::PipelineMultisampleStateCreateInfo gui_pipeline_multisample_info{
        .rasterizationSamples = vk::SampleCountFlagBits::e1, // No Multisampling
    };

    vk::PipelineDepthStencilStateCreateInfo gui_pipeline_depth_stencil_info{
        .depthCompareOp = vk::CompareOp::eAlways,
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f
    };

    vk::PipelineColorBlendAttachmentState attachment_blending{
        true, // Enable Blending
        vk::BlendFactor::eSrcAlpha, // Src Color
        vk::BlendFactor::eOneMinusSrcAlpha, // Dst Color
        vk::BlendOp::eAdd, // Color Blend Op
        vk::BlendFactor::eOne, // Src Alpha
        vk::BlendFactor::eOneMinusSrcAlpha, // Dst Alpha
        vk::BlendOp::eAdd, // Alpha Blend Op
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
    };

    vk::PipelineColorBlendStateCreateInfo gui_pipeline_blend_info{
        .attachmentCount = 1,
        .pAttachments = &attachment_blending
    };

    std::array<vk::DynamicState, 2> dynamic_states = {
        vk::DynamicState::eScissor,
        vk::DynamicState::eViewport,
    };

    vk::PipelineDynamicStateCreateInfo gui_pipeline_dynamic_info{};
    gui_pipeline_dynamic_info.setDynamicStates(dynamic_states);

    vk::GraphicsPipelineCreateInfo gui_pipeline_info{
        .pVertexInputState = &gui_pipeline_vertex_info,
        .pInputAssemblyState = &gui_pipeline_assembly_info,
        .pViewportState = &gui_pipeline_viewport_info,
        .pRasterizationState = &gui_pipeline_rasterization_info,
        .pMultisampleState = &gui_pipeline_multisample_info,
        .pDepthStencilState = &gui_pipeline_depth_stencil_info,
        .pColorBlendState = &gui_pipeline_blend_info,
        .pDynamicState = &gui_pipeline_dynamic_info,
        .layout = state.PipelineLayout,
        .renderPass = vk_state.screen_renderer.default_render_pass,
    };
    gui_pipeline_info.setStages(shader_stage_infos);

    vk::ResultValue<vk::Pipeline> pipelineResultValue = vk_state.device.createGraphicsPipeline(vk::PipelineCache(), gui_pipeline_info, nullptr);
    if (pipelineResultValue.result != vk::Result::eSuccess) {
        LOG_ERROR("Failed to create Vulkan gui pipeline.");
        return false;
    }

    state.Pipeline = pipelineResultValue.value;

    return true;
}

IMGUI_API void ImGui_ImplSdlVulkan_RenderDrawData(ImGui_VulkanState &state) {
    ImDrawData *draw_data = ImGui::GetDrawData();
    auto &vk_state = get_renderer(state);

    if (vk_state.screen_renderer.swapchain_image_idx == ~0) {
        // this happen in the game selection screen
        if (!vk_state.screen_renderer.acquire_swapchain_image(true))
            return;
    } else if (!vk_state.screen_renderer.current_cmd_buffer) {
        // Can happen while resizing the window
        return;
    }

    state.CommandBuffer = vk_state.screen_renderer.current_cmd_buffer;
    // uint32_t image_index = vk_state.screen_renderer.swapchain_image_idx;

    if (vk_state.screen_renderer.need_rebuild) {
        ImGui_ImplSdlVulkan_DeletePipeline(state);
        ImGui_ImplSdlVulkan_CreatePipeline(state);
        vk_state.screen_renderer.need_rebuild = false;
    }

    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    int fb_width = (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
    int fb_height = (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
    if (fb_width <= 0 || fb_height <= 0)
        return;

    // Allocate array to store enough vertex/index buffers
    ImGui_ImplVulkanH_WindowRenderBuffers *wrb = &state.MainWindowRenderBuffers;
    if (wrb->FrameRenderBuffers == NULL) {
        wrb->Index = 0;
        wrb->Count = vk_state.screen_renderer.swapchain_size;
        wrb->FrameRenderBuffers = (ImGui_ImplVulkanH_FrameRenderBuffers *)IM_ALLOC(sizeof(ImGui_ImplVulkanH_FrameRenderBuffers) * wrb->Count);
        memset(wrb->FrameRenderBuffers, 0, sizeof(ImGui_ImplVulkanH_FrameRenderBuffers) * wrb->Count);
    }
    IM_ASSERT(wrb->Count == vk_state.screen_renderer.swapchain_size);
    wrb->Index = (wrb->Index + 1) % wrb->Count;
    ImGui_ImplVulkanH_FrameRenderBuffers *rb = &wrb->FrameRenderBuffers[wrb->Index];

    if (draw_data->TotalVtxCount > 0) {
        // Create or resize the vertex/index buffers
        size_t vertex_size = draw_data->TotalVtxCount * sizeof(ImDrawVert);
        size_t index_size = draw_data->TotalIdxCount * sizeof(ImDrawIdx);
        if (!rb->VertexBuffer || rb->VertexBufferSize < vertex_size)
            CreateOrResizeBuffer(state, rb->VertexBuffer, rb->VertexBufferAllocation, rb->VertexBufferSize, vertex_size, vk::BufferUsageFlagBits::eVertexBuffer);
        if (!rb->IndexBuffer || rb->IndexBufferSize < index_size)
            CreateOrResizeBuffer(state, rb->IndexBuffer, rb->IndexBufferAllocation, rb->IndexBufferSize, index_size, vk::BufferUsageFlagBits::eIndexBuffer);

        // Upload vertex/index data into a single contiguous GPU buffer
        ImDrawVert *vtx_dst = static_cast<ImDrawVert *>(vk_state.allocator.mapMemory(rb->VertexBufferAllocation));
        ImDrawIdx *idx_dst = static_cast<ImDrawIdx *>(vk_state.allocator.mapMemory(rb->IndexBufferAllocation));

        for (int n = 0; n < draw_data->CmdListsCount; n++) {
            const ImDrawList *cmd_list = draw_data->CmdLists[n];
            memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
            vtx_dst += cmd_list->VtxBuffer.Size;
            idx_dst += cmd_list->IdxBuffer.Size;
        }

        vk_state.allocator.flushAllocation(rb->VertexBufferAllocation, 0, rb->VertexBufferSize);
        vk_state.allocator.flushAllocation(rb->IndexBufferAllocation, 0, rb->IndexBufferSize);

        vk_state.allocator.unmapMemory(rb->VertexBufferAllocation);
        vk_state.allocator.unmapMemory(rb->IndexBufferAllocation);
    }

    // Setup desired Vulkan state
    ImGui_ImplVulkan_SetupRenderState(state, draw_data, state.Pipeline, state.CommandBuffer, rb, fb_width, fb_height);

    // Will project scissor/clipping rectangles into framebuffer space
    ImVec2 clip_off = draw_data->DisplayPos; // (0,0) unless using multi-viewports
    ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

    // Render command lists
    // (Because we merged all buffers into a single one, we maintain our own offset into them)
    int global_vtx_offset = 0;
    int global_idx_offset = 0;
    for (int n = 0; n < draw_data->CmdListsCount; n++) {
        const ImDrawList *cmd_list = draw_data->CmdLists[n];
        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
            const ImDrawCmd *pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback != NULL) {
                // User callback, registered via ImDrawList::AddCallback()
                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                    ImGui_ImplVulkan_SetupRenderState(state, draw_data, state.Pipeline, state.CommandBuffer, rb, fb_width, fb_height);
                else
                    pcmd->UserCallback(cmd_list, pcmd);
            } else {
                // Project scissor/clipping rectangles into framebuffer space
                ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
                ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);

                // Clamp to viewport as vkCmdSetScissor() won't accept values that are off bounds
                if (clip_min.x < 0.0f) {
                    clip_min.x = 0.0f;
                }
                if (clip_min.y < 0.0f) {
                    clip_min.y = 0.0f;
                }
                if (clip_max.x > fb_width) {
                    clip_max.x = (float)fb_width;
                }
                if (clip_max.y > fb_height) {
                    clip_max.y = (float)fb_height;
                }
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;

                // Apply scissor/clipping rectangle
                vk::Rect2D scissor{
                    .offset = { static_cast<int32_t>(clip_min.x), static_cast<int32_t>(clip_min.y) },
                    .extent = { static_cast<uint32_t>(clip_max.x - clip_min.x), static_cast<uint32_t>(clip_max.y - clip_min.y) }
                };
                state.CommandBuffer.setScissor(0, scissor);

                // Bind DescriptorSet with font or user texture
                TextureState *texture;
                if (pcmd->TextureId)
                    texture = static_cast<TextureState *>(pcmd->TextureId);
                else
                    texture = state.Font;

                TextureUpdateDescriptorSet(state, texture);

                state.CommandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, state.PipelineLayout, 0, texture->descriptor_set, {});

                // Draw
                state.CommandBuffer.drawIndexed(pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0);
            }
        }
        global_idx_offset += cmd_list->IdxBuffer.Size;
        global_vtx_offset += cmd_list->VtxBuffer.Size;
    }

    // Note: at this point both vkCmdSetViewport() and vkCmdSetScissor() have been called.
    // Our last values will leak into user/application rendering IF:
    // - Your app uses a pipeline with VK_DYNAMIC_STATE_VIEWPORT or VK_DYNAMIC_STATE_SCISSOR dynamic state
    // - And you forgot to call vkCmdSetViewport() and vkCmdSetScissor() yourself to explicitely set that state.
    // If you use VK_DYNAMIC_STATE_VIEWPORT or VK_DYNAMIC_STATE_SCISSOR you are responsible for setting the values before rendering.
    // In theory we should aim to backup/restore those values but I am not sure this is possible.
    // We perform a call to vkCmdSetScissor() to set back a full viewport which is likely to fix things for 99% users but technically this is not perfect. (See github #4644)

    // not necessary, we are not using a dynamic viewport
    // VkRect2D scissor = { { 0, 0 }, { (uint32_t)fb_width, (uint32_t)fb_height } };
    // vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    state.frame_timestamp++;
}

IMGUI_API ImTextureID ImGui_ImplSdlVulkan_CreateTexture(ImGui_VulkanState &state, void *pixels, int width, int height, bool is_alpha) {
    auto *texture = new TextureState;

    const size_t buffer_size = width * height * (is_alpha ? 1 : 4);

    vk::BufferCreateInfo buffer_info{
        .size = buffer_size,
        .usage = vk::BufferUsageFlagBits::eTransferSrc,
        .sharingMode = vk::SharingMode::eExclusive,
    };

    auto &vk_state = get_renderer(state);

    vma::AllocationInfo alloc_info;
    auto [temp_buffer, temp_allocation] = vk_state.allocator.createBuffer(buffer_info, vkutil::vma_mapped_alloc, alloc_info);

    vk_state.allocator.invalidateAllocation(temp_allocation, 0, buffer_size);

    std::memcpy(alloc_info.pMappedData, pixels, buffer_size);

    vk_state.allocator.flushAllocation(temp_allocation, 0, buffer_size);

    const vk::Format format = is_alpha ? vk::Format::eR8Unorm : vk::Format::eR8G8B8A8Unorm;

    vk::ImageCreateInfo image_info{
        .imageType = vk::ImageType::e2D,
        .format = format,
        .extent = vk::Extent3D{ static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vk::SampleCountFlagBits::e1,
        .tiling = vk::ImageTiling::eOptimal,
        .usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        .sharingMode = vk::SharingMode::eExclusive,
        .initialLayout = vk::ImageLayout::eUndefined
    };

    std::tie(texture->image, texture->allocation) = vk_state.allocator.createImage(image_info, vkutil::vma_auto_alloc);

    vk::CommandBuffer transfer_buffer = vkutil::create_single_time_command(vk_state.device,
        vk_state.transfer_command_pool);

    vk::ImageMemoryBarrier image_transfer_optimal_barrier{
        .srcAccessMask = vk::AccessFlagBits(),
        .dstAccessMask = vk::AccessFlagBits::eTransferWrite, // Will be written by a transfer operation.
        .oldLayout = vk::ImageLayout::eUndefined, // Old Layout
        .newLayout = vk::ImageLayout::eTransferDstOptimal, // New Layout
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, // No Queue Family Transition
        .image = texture->image,
        .subresourceRange = vkutil::color_subresource_range // Subresource Range
    };

    transfer_buffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, // Top Of Pipe -> Transfer Stage
        vk::DependencyFlags(), // No Dependency Flags
        0, nullptr, // No Memory Barriers
        0, nullptr, // No Buffer Memory Barriers
        1, &image_transfer_optimal_barrier // 1 Image Memory Barrier
    );

    vk::BufferImageCopy region{
        0, // Buffer Offset
        static_cast<uint32_t>(width), // Buffer Row Length
        static_cast<uint32_t>(height), // Buffer Height
        vk::ImageSubresourceLayers{
            vk::ImageAspectFlagBits::eColor, // Aspects
            0, 0, 1 // First Layer/Level
        },
        vk::Offset3D{ 0, 0, 0 }, // Image Offset
        vk::Extent3D{ static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 } // Image Extent
    };

    transfer_buffer.copyBufferToImage(
        temp_buffer, // Buffer
        texture->image, // Image
        vk::ImageLayout::eTransferDstOptimal, // Image Layout
        1, &region // Regions
    );

    vk::ImageMemoryBarrier image_shader_read_only_barrier{
        .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
        .dstAccessMask = vk::AccessFlagBits(),
        .oldLayout = vk::ImageLayout::eTransferDstOptimal,
        .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = texture->image,
        .subresourceRange = vkutil::color_subresource_range
    };

    transfer_buffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eBottomOfPipe, // Transfer -> Bottom of Pipe Stage
        vk::DependencyFlags(), // No Dependency Flags
        0, nullptr, // No Memory Barriers
        0, nullptr, // No Buffer Barriers
        1, &image_shader_read_only_barrier // Image Memory Barriers
    );

    vkutil::end_single_time_command(vk_state.device, vk_state.transfer_queue, vk_state.transfer_command_pool, transfer_buffer);
    vk_state.allocator.destroyBuffer(temp_buffer, temp_allocation);

    const vk::ComponentMapping mapping = is_alpha ? vk::ComponentMapping{ vk::ComponentSwizzle::eOne, vk::ComponentSwizzle::eOne, vk::ComponentSwizzle::eOne, vk::ComponentSwizzle::eR }
                                                  : vkutil::default_comp_mapping;

    vk::ImageViewCreateInfo font_view_info{
        .image = texture->image,
        .viewType = vk::ImageViewType::e2D,
        .format = format,
        .components = mapping,
        .subresourceRange = vkutil::color_subresource_range
    };

    texture->image_view = vk_state.device.createImageView(font_view_info);

    return texture;
}

IMGUI_API void ImGui_ImplSdlVulkan_DeleteTexture(ImGui_VulkanState &state, ImTextureID texture) {
    auto texture_ptr = static_cast<TextureState *>(texture);
    auto &vk_state = get_renderer(state);

    vk_state.device.waitIdle();
    vk_state.device.destroy(texture_ptr->image_view);
    vk_state.allocator.destroyImage(texture_ptr->image, texture_ptr->allocation);

    delete texture_ptr;
}

// Use if you want to reset your rendering device without losing ImGui state.
IMGUI_API void ImGui_ImplSdlVulkan_InvalidateDeviceObjects(ImGui_VulkanState &state) {
    auto &vk_state = get_renderer(state);

    ImGui_ImplSdlVulkan_DeleteTexture(state, state.Font);
    ImGui_ImplSdlVulkan_DeletePipeline(state);

    vk_state.device.destroy(state.PipelineLayout);
    vk_state.device.destroy(state.DescriptorSetLayout);

    vk_state.device.destroy(state.FontSampler);

    vk_state.device.destroy(state.DescriptorPool);
}

IMGUI_API bool ImGui_ImplSdlVulkan_CreateDeviceObjects(ImGui_VulkanState &state) {
    auto &vk_state = get_renderer(state);

    if (!state.FontSampler) {
        vk::SamplerCreateInfo info{
            .magFilter = vk::Filter::eLinear,
            .minFilter = vk::Filter::eLinear,
            .mipmapMode = vk::SamplerMipmapMode::eLinear,
            .addressModeU = vk::SamplerAddressMode::eRepeat,
            .addressModeV = vk::SamplerAddressMode::eRepeat,
            .addressModeW = vk::SamplerAddressMode::eRepeat,
            .minLod = -1000,
            .maxLod = 1000,
        };
        state.FontSampler = vk_state.device.createSampler(info);
    }

    if (!state.DescriptorSetLayout) {
        vk::DescriptorSetLayoutBinding binding{
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eFragment,
        };
        binding.setImmutableSamplers(state.FontSampler);
        vk::DescriptorSetLayoutCreateInfo info{};
        info.setBindings(binding);
        state.DescriptorSetLayout = vk_state.device.createDescriptorSetLayout(info);
    }

    if (!state.PipelineLayout) {
        // Constants: we are using 'vec2 offset' and 'vec2 scale' instead of a full 3d projection matrix
        vk::PushConstantRange push_constants{
            .stageFlags = vk::ShaderStageFlagBits::eVertex,
            .offset = sizeof(float) * 0,
            .size = sizeof(float) * 4
        };
        vk::PipelineLayoutCreateInfo layout_info{};
        layout_info.setSetLayouts(state.DescriptorSetLayout);
        layout_info.setPushConstantRanges(push_constants);
        state.PipelineLayout = vk_state.device.createPipelineLayout(layout_info);
    }

    ImGui_ImplSdlVulkan_CreatePipeline(state);

    {
        // Hopefully imgui doesn't use more than 2048 different images
        // within k frames (where k is the number of images in the swapchain)
        vk::DescriptorPoolSize pool_size{
            .type = vk::DescriptorType::eCombinedImageSampler,
            .descriptorCount = TextureState::nb_descriptor_sets + 1
        };
        vk::DescriptorPoolCreateInfo pool_info{
            .maxSets = TextureState::nb_descriptor_sets + 1,
        };
        pool_info.setPoolSizes(pool_size);
        state.DescriptorPool = vk_state.device.createDescriptorPool(pool_info);
        vk::DescriptorSetAllocateInfo descr_set_info{
            .descriptorPool = state.DescriptorPool,
        };
        std::vector<vk::DescriptorSetLayout> descr_set_layouts(TextureState::nb_descriptor_sets + 1, state.DescriptorSetLayout);
        descr_set_info.setSetLayouts(descr_set_layouts);
        state.DescriptorSets = vk_state.device.allocateDescriptorSets(descr_set_info);
    }

    // Create ImGui Texture
    {
        ImGuiIO &io = ImGui::GetIO();

        uint8_t *pixels;
        int width, height;
        io.Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);

        io.Fonts->TexID = ImGui_ImplSdlVulkan_CreateTexture(state, pixels, width, height, true);
        state.Font = static_cast<TextureState *>(io.Fonts->TexID);

        // reserve and remove the last descriptor set for the font
        state.Font->descriptor_set = state.DescriptorSets[TextureState::nb_descriptor_sets];
        state.DescriptorSets.resize(TextureState::nb_descriptor_sets);

        // update this descriptor setonce and for all
        vk::DescriptorImageInfo descr_image_info{
            .imageView = state.Font->image_view,
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        };
        vk::WriteDescriptorSet write_descr{
            .dstSet = state.Font->descriptor_set,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        };
        write_descr.setImageInfo(descr_image_info);
        get_renderer(state).device.updateDescriptorSets(write_descr, {});
    }

    state.init = true;

    return true;
}
