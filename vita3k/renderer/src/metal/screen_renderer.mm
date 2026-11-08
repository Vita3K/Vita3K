// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#include "screen_renderer.h"

#include "mtl_types.h"
#include "shader_translate.h"

#include <renderer/metal/state.h>
#include <renderer/types.h>
#include <util/log.h>
#include <vkutil/native_surface.h>

#include <cmath>

namespace renderer::metal {

namespace {

struct screen_vertex {
    float pos[3];
    float uv[2];
};

constexpr size_t screen_vertex_size = sizeof(screen_vertex);
constexpr uint32_t screen_vertex_count = 4;

using screen_vertices_t = screen_vertex[screen_vertex_count];

struct EasuConstant {
    Viewport viewport;
    uint32_t output_width;
    uint32_t output_height;
};

struct RcasConstant {
    uint32_t offset_x;
    uint32_t offset_y;
    float sharpening;
};

id<MTLFunction> compile_builtin(MetalState &state, const char *name, msl::Stage stage, const std::vector<msl::BindingRemap> &remaps) {
    const fs::path path = state.static_assets / "shaders-builtin" / "vulkan" / name;
    const std::vector<uint32_t> spirv = msl::load_spirv_file(path);
    if (spirv.empty()) {
        LOG_ERROR("Failed to load builtin shader {}", path.string());
        return nil;
    }

    const msl::TranslateResult result = msl::translate_builtin_shader(spirv, stage, remaps, msl::BUILTIN_PUSH_CONSTANT_BUFFER_INDEX);
    if (!result.ok) {
        LOG_ERROR("Failed to translate builtin shader {} to MSL: {}", name, result.error);
        return nil;
    }

    NSError *error = nil;
    MTLCompileOptions *options = [[MTLCompileOptions alloc] init];
    options.languageVersion = MTLLanguageVersion2_3;
    id<MTLLibrary> library = [state.device->device newLibraryWithSource:[NSString stringWithUTF8String:result.source.c_str()] options:options error:&error];
    if (!library) {
        LOG_ERROR("Failed to compile builtin shader {}: {}", name, error ? error.localizedDescription.UTF8String : "unknown error");
        return nil;
    }

    id<MTLFunction> function = [library newFunctionWithName:[NSString stringWithUTF8String:result.entry_point.c_str()]];
    if (!function && library.functionNames.count > 0)
        function = [library newFunctionWithName:library.functionNames[0]];
    return function;
}

id<MTLSamplerState> create_screen_sampler(MetalState &state, MTLSamplerMinMagFilter filter) {
    MTLSamplerDescriptor *desc = [[MTLSamplerDescriptor alloc] init];
    desc.magFilter = filter;
    desc.minFilter = filter;
    desc.mipFilter = MTLSamplerMipFilterNotMipmapped;
    desc.sAddressMode = MTLSamplerAddressModeClampToEdge;
    desc.tAddressMode = MTLSamplerAddressModeClampToEdge;
    desc.rAddressMode = MTLSamplerAddressModeClampToEdge;
    return [state.device->device newSamplerStateWithDescriptor:desc];
}

CAMetalLayer *get_window_layer(MetalState &state) {
    renderer::FrameHost *frame_host = state.frame;
    void *layer_ptr = frame_host->metal_layer();
    if (!layer_ptr) {
        const renderer::DisplayHandle display_handle = frame_host->handle();
        if (const auto *handle = std::get_if<renderer::MacOSDisplayHandle>(&display_handle))
            layer_ptr = get_metal_layer_from_view(handle->view);
    }
    if (!layer_ptr)
        return nil;

    id object = (__bridge id)layer_ptr;
    if (![object isKindOfClass:[CAMetalLayer class]]) {
        LOG_ERROR("The window layer is not a CAMetalLayer");
        return nil;
    }
    return (CAMetalLayer *)object;
}

// compute the area of the window where the game is displayed
void compute_output_area(ScreenRenderer &screen, float &x, float &y, float &w, float &h) {
    const float window_aspect = static_cast<float>(screen.width) / screen.height;
    constexpr float vita_aspect = static_cast<float>(DEFAULT_RES_WIDTH) / DEFAULT_RES_HEIGHT;
    const bool pixel_perfect = screen.state.fullscreen_hd_res_pixel_perfect && screen.state.fullscreen
        && !(screen.width % DEFAULT_RES_WIDTH) && !(screen.height % (DEFAULT_RES_HEIGHT - 4));
    if (screen.state.stretch_the_display_area && !pixel_perfect) {
        // Match the aspect ratio to the screen size.
        w = static_cast<float>(screen.width);
        h = static_cast<float>(screen.height);
        x = 0.0f;
        y = 0.0f;
    } else if ((window_aspect > vita_aspect) && !pixel_perfect) {
        // Window is wide. Pin top and bottom.
        w = screen.height * vita_aspect;
        h = static_cast<float>(screen.height);
        x = (screen.width - w) / 2.0f;
        y = 0.0f;
    } else {
        // Window is tall. Pin left and right.
        w = static_cast<float>(screen.width);
        h = screen.width / vita_aspect;
        x = 0.0f;
        y = (screen.height - h) / 2.0f;
    }
}

} // namespace

ScreenRenderer::ScreenRenderer(MetalState &state)
    : state(state) {
}

bool ScreenRenderer::create() {
    layer = get_window_layer(state);
    if (!layer) {
        LOG_ERROR("Failed to get the CAMetalLayer of the window");
        return false;
    }

    layer.device = state.device->device;
    layer.pixelFormat = format;
    // the FSR filter writes to the drawable with a compute shader
    layer.framebufferOnly = NO;
    layer.maximumDrawableCount = 3;
    layer.displaySyncEnabled = vsync;
    update_drawable_size();

    set_filter("Bilinear");
    return filter != nullptr;
}

void ScreenRenderer::cleanup() {
    if (current_encoder) {
        [current_encoder endEncoding];
        current_encoder = nil;
    }
    current_cmd_buffer = nil;
    drawable = nil;

    for (auto &cmd : slot_cmds) {
        if (cmd)
            [cmd waitUntilCompleted];
        cmd = nil;
    }

    filter.reset();
    for (auto &texture : vita_surface)
        texture = nil;
    layer = nil;
}

bool ScreenRenderer::update_drawable_size() {
    const int new_width = state.frame->drawable_width();
    const int new_height = state.frame->drawable_height();
    if (new_width <= 0 || new_height <= 0)
        return false;

    if (static_cast<uint32_t>(new_width) != width || static_cast<uint32_t>(new_height) != height) {
        width = static_cast<uint32_t>(new_width);
        height = static_cast<uint32_t>(new_height);
        layer.drawableSize = CGSizeMake(width, height);
        if (filter)
            filter->on_resize();
        return true;
    }
    return false;
}

bool ScreenRenderer::acquire_drawable() {
    if (!layer)
        return false;

    update_drawable_size();
    if (width == 0 || height == 0)
        return false;

    current_slot = (current_slot + 1) % frame_slots;
    // make sure the resources of this slot are no longer in use
    if (slot_cmds[current_slot]) {
        [slot_cmds[current_slot] waitUntilCompleted];
        slot_cmds[current_slot] = nil;
    }

    drawable = [layer nextDrawable];
    if (!drawable) {
        LOG_WARN_ONCE("Could not get a drawable from the CAMetalLayer");
        return false;
    }

    current_cmd_buffer = [state.device->queue commandBuffer];
    current_cmd_buffer.label = @"present";
    current_encoder = nil;
    return true;
}

void ScreenRenderer::begin_render_pass(bool clear) {
    if (current_encoder || !current_cmd_buffer || !drawable)
        return;

    MTLRenderPassDescriptor *desc = [MTLRenderPassDescriptor renderPassDescriptor];
    desc.colorAttachments[0].texture = drawable.texture;
    desc.colorAttachments[0].loadAction = clear ? MTLLoadActionClear : MTLLoadActionLoad;
    desc.colorAttachments[0].storeAction = MTLStoreActionStore;
    desc.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);

    current_encoder = [current_cmd_buffer renderCommandEncoderWithDescriptor:desc];
    current_encoder.label = @"screen";
}

void ScreenRenderer::render(id<MTLTexture> texture, const Viewport &viewport) {
    if (!current_cmd_buffer || !drawable)
        return;

    // compute filters run before the render pass
    filter->render(true, texture, viewport);

    begin_render_pass(!filter->need_post_processing_render_pass());

    filter->render(false, texture, viewport);
}

void ScreenRenderer::swap_window() {
    // the vsync setting can be changed from the gui
    const int pending_vsync = state.pending_vsync.exchange(-1, std::memory_order_relaxed);
    if (pending_vsync >= 0)
        set_vsync(pending_vsync != 0);

    if (!current_cmd_buffer) {
        drawable = nil;
        return;
    }

    if (current_encoder) {
        [current_encoder endEncoding];
        current_encoder = nil;
    }

    if (drawable)
        [current_cmd_buffer presentDrawable:drawable];
    [current_cmd_buffer commit];
    slot_cmds[current_slot] = current_cmd_buffer;

    current_cmd_buffer = nil;
    drawable = nil;
}

void ScreenRenderer::set_vsync(bool enable) {
    vsync = enable;
    if (layer)
        layer.displaySyncEnabled = enable;
}

void ScreenRenderer::set_filter(const std::string_view &filter_name) {
    if (this->filter && filter_name == this->filter->get_name())
        return;

    // wait for the current frames to be done, the filter resources are about to be released
    for (auto &cmd : slot_cmds) {
        if (cmd)
            [cmd waitUntilCompleted];
        cmd = nil;
    }

    this->filter.reset();
    if (filter_name == "FSR")
        this->filter = std::make_unique<FSRScreenFilter>(*this);
    else if (filter_name == "FXAA")
        this->filter = std::make_unique<FXAAScreenFilter>(*this);
    else if (filter_name == "Bicubic")
        this->filter = std::make_unique<BicubicScreenFilter>(*this);
    else if (filter_name == "Nearest")
        this->filter = std::make_unique<NearestScreenFilter>(*this);
    else
        this->filter = std::make_unique<BilinearScreenFilter>(*this);

    if (!this->filter->init()) {
        LOG_ERROR("Failed to initialize the {} screen filter, falling back to bilinear filtering", filter_name);
        this->filter = std::make_unique<BilinearScreenFilter>(*this);
        if (!this->filter->init())
            this->filter.reset();
    }
}

// Single pass filters

std::string_view SinglePassScreenFilter::get_vertex_name() {
    return "render_main.vert.spv";
}

std::string_view SinglePassScreenFilter::get_fragment_name() {
    return "render_main.frag.spv";
}

bool SinglePassScreenFilter::init() {
    @autoreleasepool {
        MetalState &state = screen.state;
        id<MTLFunction> vertex_function = compile_builtin(state, get_vertex_name().data(), msl::Stage::Vertex, {});
        id<MTLFunction> fragment_function = compile_builtin(state, get_fragment_name().data(), msl::Stage::Fragment, { { 0, 0, 0, 0, 0 } });
        if (!vertex_function || !fragment_function)
            return false;

        MTLVertexDescriptor *vertex_descriptor = [MTLVertexDescriptor vertexDescriptor];
        vertex_descriptor.attributes[0].format = MTLVertexFormatFloat3;
        vertex_descriptor.attributes[0].offset = offsetof(screen_vertex, pos);
        vertex_descriptor.attributes[0].bufferIndex = msl::BUILTIN_VERTEX_BUFFER_INDEX;
        vertex_descriptor.attributes[1].format = MTLVertexFormatFloat2;
        vertex_descriptor.attributes[1].offset = offsetof(screen_vertex, uv);
        vertex_descriptor.attributes[1].bufferIndex = msl::BUILTIN_VERTEX_BUFFER_INDEX;
        vertex_descriptor.layouts[msl::BUILTIN_VERTEX_BUFFER_INDEX].stride = screen_vertex_size;
        vertex_descriptor.layouts[msl::BUILTIN_VERTEX_BUFFER_INDEX].stepFunction = MTLVertexStepFunctionPerVertex;

        MTLRenderPipelineDescriptor *desc = [[MTLRenderPipelineDescriptor alloc] init];
        desc.vertexFunction = vertex_function;
        desc.fragmentFunction = fragment_function;
        desc.vertexDescriptor = vertex_descriptor;
        desc.colorAttachments[0].pixelFormat = screen.format;
        desc.colorAttachments[0].blendingEnabled = NO;

        NSError *error = nil;
        pipeline = [state.device->device newRenderPipelineStateWithDescriptor:desc error:&error];
        if (!pipeline) {
            LOG_ERROR("Failed to create the screen pipeline: {}", error ? error.localizedDescription.UTF8String : "unknown error");
            return false;
        }

        sampler = create_sampler();

        vertex_buffers.resize(ScreenRenderer::frame_slots);
        for (auto &buffer : vertex_buffers)
            buffer = create_shared_buffer(state.device->device, sizeof(screen_vertices_t), "screen vertices");
        last_uvs.resize(ScreenRenderer::frame_slots);
        std::ranges::fill(last_uvs, std::array<float, 4>{ -1.0f, -1.0f, -1.0f, -1.0f });

        return true;
    }
}

void SinglePassScreenFilter::render(bool is_pre_renderpass, id<MTLTexture> src, const Viewport &viewport) {
    if (is_pre_renderpass)
        return;

    id<MTLRenderCommandEncoder> encoder = screen.current_encoder;
    if (!encoder || !pipeline)
        return;

    const uint32_t slot = screen.current_slot;
    const std::array<float, 4> uvs = {
        viewport.offset_x / static_cast<float>(viewport.texture_width),
        viewport.offset_y / static_cast<float>(viewport.texture_height),
        (viewport.offset_x + viewport.width) / static_cast<float>(viewport.texture_width),
        (viewport.offset_y + viewport.height) / static_cast<float>(viewport.texture_height)
    };

    // if necessary update the vertices (should not happen often)
    if (uvs != last_uvs[slot]) {
        screen_vertices_t vertex_buffer_data = {
            { { 1.f, -1.f, 0.0f }, { uvs[2], uvs[1] } },
            { { -1.f, -1.f, 0.0f }, { uvs[0], uvs[1] } },
            { { 1.f, 1.f, 0.0f }, { uvs[2], uvs[3] } },
            { { -1.f, 1.f, 0.0f }, { uvs[0], uvs[3] } },
        };
        memcpy(vertex_buffers[slot].contents, &vertex_buffer_data, sizeof(vertex_buffer_data));
        last_uvs[slot] = uvs;
    }

    float x, y, w, h;
    compute_output_area(screen, x, y, w, h);

    [encoder setViewport:MTLViewport{ x, y, w, h, 0.0, 1.0 }];
    [encoder setScissorRect:MTLScissorRect{ 0, 0, screen.width, screen.height }];
    [encoder setCullMode:MTLCullModeNone];
    [encoder setTriangleFillMode:MTLTriangleFillModeFill];
    [encoder setRenderPipelineState:pipeline];
    [encoder setVertexBuffer:vertex_buffers[slot] offset:0 atIndex:msl::BUILTIN_VERTEX_BUFFER_INDEX];
    [encoder setFragmentTexture:src atIndex:0];
    [encoder setFragmentSamplerState:sampler atIndex:0];

    const std::array<float, 2> inv_size = { 1.f / viewport.texture_width, 1.f / viewport.texture_height };
    [encoder setFragmentBytes:inv_size.data() length:sizeof(inv_size) atIndex:msl::BUILTIN_PUSH_CONSTANT_BUFFER_INDEX];

    [encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:screen_vertex_count];
}

id<MTLSamplerState> NearestScreenFilter::create_sampler() {
    return create_screen_sampler(screen.state, MTLSamplerMinMagFilterNearest);
}

id<MTLSamplerState> BilinearScreenFilter::create_sampler() {
    return create_screen_sampler(screen.state, MTLSamplerMinMagFilterLinear);
}

std::string_view BicubicScreenFilter::get_fragment_name() {
    return "render_main_bicubic.frag.spv";
}

id<MTLSamplerState> BicubicScreenFilter::create_sampler() {
    return create_screen_sampler(screen.state, MTLSamplerMinMagFilterLinear);
}

std::string_view FXAAScreenFilter::get_fragment_name() {
    return "render_main_fxaa.frag.spv";
}

id<MTLSamplerState> FXAAScreenFilter::create_sampler() {
    return create_screen_sampler(screen.state, MTLSamplerMinMagFilterLinear);
}

// FSR

bool FSRScreenFilter::init() {
    @autoreleasepool {
        MetalState &state = screen.state;

        // set 0: binding 1 input texture, binding 2 output image, binding 3 sampler
        const std::vector<msl::BindingRemap> remaps = {
            { 0, 1, 0, 0, 0 },
            { 0, 2, 0, 1, 0 },
            { 0, 3, 0, 0, 0 },
        };
        id<MTLFunction> easu_function = compile_builtin(state, "fsr_filter_easu.comp.spv", msl::Stage::Compute, remaps);
        id<MTLFunction> rcas_function = compile_builtin(state, "fsr_filter_rcas.comp.spv", msl::Stage::Compute, remaps);
        if (!easu_function || !rcas_function)
            return false;

        NSError *error = nil;
        pipeline_easu = [state.device->device newComputePipelineStateWithFunction:easu_function error:&error];
        if (!pipeline_easu) {
            LOG_ERROR("Failed to create the FSR EASU pipeline: {}", error ? error.localizedDescription.UTF8String : "unknown error");
            return false;
        }
        pipeline_rcas = [state.device->device newComputePipelineStateWithFunction:rcas_function error:&error];
        if (!pipeline_rcas) {
            LOG_ERROR("Failed to create the FSR RCAS pipeline: {}", error ? error.localizedDescription.UTF8String : "unknown error");
            return false;
        }

        sampler = create_screen_sampler(state, MTLSamplerMinMagFilterLinear);
        intermediate_images.resize(ScreenRenderer::frame_slots);

        on_resize();
        return true;
    }
}

void FSRScreenFilter::on_resize() {
    float x, y, w, h;
    compute_output_area(screen, x, y, w, h);
    output_offset_x = static_cast<uint32_t>(std::round(x));
    output_offset_y = static_cast<uint32_t>(std::round(y));
    output_width = std::max<uint32_t>(static_cast<uint32_t>(std::round(w)), 1);
    output_height = std::max<uint32_t>(static_cast<uint32_t>(std::round(h)), 1);

    // recreate the intermediate images
    for (auto &image : intermediate_images) {
        image = create_texture(screen.state.device->device, MTLPixelFormatRGBA8Unorm, output_width, output_height,
            MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite, 1, false, 1, MTLStorageModePrivate, "fsr intermediate");
    }
}

void FSRScreenFilter::render(bool is_pre_renderpass, id<MTLTexture> src, const Viewport &viewport) {
    if (!is_pre_renderpass)
        // we are using compute shaders
        return;

    if (!screen.current_cmd_buffer || !screen.drawable)
        return;

    const uint32_t slot = screen.current_slot;
    id<MTLTexture> intermediate = intermediate_images[slot];
    id<MTLTexture> output = screen.drawable.texture;

    const uint32_t dispatch_x = (output_width + 15) / 16;
    const uint32_t dispatch_y = (output_height + 15) / 16;

    // the drawable must be cleared as the sharpening pass only writes the output area
    {
        MTLRenderPassDescriptor *desc = [MTLRenderPassDescriptor renderPassDescriptor];
        desc.colorAttachments[0].texture = output;
        desc.colorAttachments[0].loadAction = MTLLoadActionClear;
        desc.colorAttachments[0].storeAction = MTLStoreActionStore;
        desc.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
        id<MTLRenderCommandEncoder> clear_encoder = [screen.current_cmd_buffer renderCommandEncoderWithDescriptor:desc];
        [clear_encoder endEncoding];
    }

    id<MTLComputeCommandEncoder> encoder = [screen.current_cmd_buffer computeCommandEncoder];
    encoder.label = @"fsr";

    // upscaling pass
    [encoder setComputePipelineState:pipeline_easu];
    [encoder setTexture:src atIndex:0];
    [encoder setTexture:intermediate atIndex:1];
    [encoder setSamplerState:sampler atIndex:0];
    const EasuConstant easu_constant{
        .viewport = viewport,
        .output_width = output_width,
        .output_height = output_height
    };
    [encoder setBytes:&easu_constant length:sizeof(easu_constant) atIndex:msl::BUILTIN_PUSH_CONSTANT_BUFFER_INDEX];
    [encoder dispatchThreadgroups:MTLSizeMake(dispatch_x, dispatch_y, 1) threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];

    // sharpening pass
    [encoder setComputePipelineState:pipeline_rcas];
    [encoder setTexture:intermediate atIndex:0];
    [encoder setTexture:output atIndex:1];
    [encoder setSamplerState:sampler atIndex:0];
    const RcasConstant rcas_constant{
        .offset_x = output_offset_x,
        .offset_y = output_offset_y,
        // some default value for sharpening
        .sharpening = 0.2f
    };
    [encoder setBytes:&rcas_constant length:sizeof(rcas_constant) atIndex:msl::BUILTIN_PUSH_CONSTANT_BUFFER_INDEX];
    [encoder dispatchThreadgroups:MTLSizeMake(dispatch_x, dispatch_y, 1) threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];

    [encoder endEncoding];
}

} // namespace renderer::metal
