// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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

#include <renderer/functions.h>
#include <renderer/types.h>
#include <renderer/vulkan/functions.h>
#include <renderer/vulkan/state.h>

#include <config/version.h>
#include <display/state.h>
#include <shader/spirv_recompiler.h>
#include <util/float_to_half.h>
#include <util/log.h>
#include <vkutil/vkutil.h>

#include <SDL_vulkan.h>

static vk::DebugUtilsMessengerEXT debug_messenger;

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
    void *pUserData) {
    if (message_severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
        // for now we are not interested by performance warnings
        && (message_type & ~VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)) {
        std::string_view message = callback_data->pMessage;
        // ignore this message for now
        if (message.find("VUID-vkCmdDrawIndexed-None-02721") == std::string_view::npos)
            LOG_ERROR("Validation layer: {}", callback_data->pMessage);
    }
    return VK_FALSE;
}

const static std::vector<const char *> required_device_extensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    // needed in order to use storage buffers
    VK_KHR_STORAGE_BUFFER_STORAGE_CLASS_EXTENSION_NAME,
    // needed in order to use negative viewport height
    VK_KHR_MAINTENANCE1_EXTENSION_NAME
};

namespace renderer::vulkan {

static bool device_is_compatible(const vk::PhysicalDevice &device) {
    const std::vector<vk::ExtensionProperties> available_extensions = device.enumerateDeviceExtensionProperties();

    std::set<std::string> required_extensions(required_device_extensions.begin(), required_device_extensions.end());
    for (const auto &extension : available_extensions)
        required_extensions.erase(extension.extensionName);

    return required_extensions.empty();
}

static bool select_queues(VKState &vk_state,
    std::vector<vk::DeviceQueueCreateInfo> &queue_infos, std::vector<std::vector<float>> &queue_priorities) {
    // TODO: Better queue allocation.

    /**
     * Here's the idea:
     *  - Dedicated queues to a task (e.g. with only graphics bit set) are faster.
     *  - Queues that appear first in the list are faster.
     *  - We really just need a queue for Graphics and Transfer right now afaik.
     *  - Multiple queues families can do the same thing.
     *  - Multiple different queues should be chosen if available.
     * The current algorithm only picks the first one it finds, a new algorithm should be made that takes everything into account.
     */

    bool found_graphics = false, found_transfer = false;

    for (uint32_t i = 0; i < vk_state.physical_device_queue_families.size(); i++) {
        const auto &queue_family = vk_state.physical_device_queue_families[i];

        // MoltenVK does not accept nullptr a pPriorities for some reason.
        std::vector<float> &priorities = queue_priorities.emplace_back(queue_family.queueCount, 1.0f);

        // Only one DeviceQueueCreateInfo should be created per family.
        if (!found_graphics && (queue_family.queueFlags & vk::QueueFlagBits::eGraphics)
            && (queue_family.queueFlags & vk::QueueFlagBits::eTransfer)
            && vk_state.physical_device.getSurfaceSupportKHR(i, vk_state.screen_renderer.surface)) {
            vk::DeviceQueueCreateInfo queue_create_info{
                .queueFamilyIndex = i,
                .queueCount = queue_family.queueCount,
                .pQueuePriorities = priorities.data()
            };
            queue_infos.emplace_back(std::move(queue_create_info));
            vk_state.general_family_index = i;
            vk_state.transfer_family_index = i;
            found_graphics = true;
            found_transfer = true;
        }
        // for now use the same queue for graphics and transfer, to be improved on later
        /* else if (!found_transfer && queue_family.queueFlags & vk::QueueFlagBits::eTransfer) {
            vk::DeviceQueueCreateInfo queue_create_info{
                .queueFamilyIndex = i,
                .queueCount = queue_family.queueCount,
                .pQueuePriorities = priorities.data()
            };
            queue_infos.emplace_back(std::move(queue_create_info));
            vk_state.transfer_family_index = i;
            found_transfer = true;
        }
        */

        if (found_graphics && found_transfer)
            break;
    }

    return found_graphics && found_transfer;
}

// Adapted from https://github.com/SaschaWillems/vulkan.gpuinfo.org/blob/master/includes/functions.php
std::string get_driver_version(uint32_t vendor_id, uint32_t version_raw) {
    // NVIDIA
    if (vendor_id == 4318)
        return fmt::format("{}.{}.{}.{}", (version_raw >> 22) & 0x3ff, (version_raw >> 14) & 0x0ff, (version_raw >> 6) & 0x0ff, (version_raw)&0x003f);

#ifdef WIN32
    // Intel drivers on Windows
    if (vendor_id == 0x8086)
        return fmt::format("{}.{}", version_raw >> 14, (version_raw)&0x3fff);
#endif

    // Use Vulkan version conventions if vendor mapping is not available
    return fmt::format("{}.{}.{}", (version_raw >> 22) & 0x3ff, (version_raw >> 12) & 0x3ff, (version_raw)&0xfff);
}

bool create(SDL_Window *window, std::unique_ptr<renderer::State> &state, const char *base_path) {
    auto &vk_state = dynamic_cast<VKState &>(*state);

    return vk_state.create(window, state, base_path);
}

VKState::VKState(int gpu_idx)
    : gpu_idx(gpu_idx)
    , surface_cache(*this)
    , pipeline_cache(*this)
    , texture_cache(*this)
    , screen_renderer(*this) {
}

bool VKState::init(const char *base_path, const bool hashless_texture_cache) {
    shader_version = fmt::format("v{}", shader::CURRENT_VERSION);
    return true;
}

bool VKState::create(SDL_Window *window, std::unique_ptr<renderer::State> &state, const char *base_path) {
    // Create Instance
    {
        PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_Vulkan_GetVkGetInstanceProcAddr());
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

        vk::ApplicationInfo app_info{
            .pApplicationName = app_name, // App Name
            .applicationVersion = VK_MAKE_API_VERSION(0, 0, 0, 1), // App Version
            .pEngineName = org_name, // Engine Name, using org instead.
            .engineVersion = VK_MAKE_API_VERSION(0, 0, 0, 1), // Engine Version
            .apiVersion = VK_API_VERSION_1_0
        };

        unsigned int instance_req_ext_count;
        if (!SDL_Vulkan_GetInstanceExtensions(window, &instance_req_ext_count, nullptr)) {
            LOG_ERROR("Could not get required extensions");
            return false;
        }

        std::vector<const char *> instance_extensions;
        instance_extensions.resize(instance_req_ext_count);
        SDL_Vulkan_GetInstanceExtensions(window, &instance_req_ext_count, instance_extensions.data());

        const std::set<std::string> optional_instance_extensions = {
            VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
            VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
        };
        for (const vk::ExtensionProperties &prop : vk::enumerateInstanceExtensionProperties()) {
            auto ite = optional_instance_extensions.find(prop.extensionName);
            if (ite != optional_instance_extensions.end()) {
                instance_extensions.push_back(ite->c_str());
            }
        }

        // look if we can use the validation layer
        bool has_debug_extension = false;
        bool has_validation_layer = false;
        const std::string debug_extension = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        for (const vk::ExtensionProperties &prop : vk::enumerateInstanceExtensionProperties()) {
            if (std::string(prop.extensionName.data()) == debug_extension) {
                has_debug_extension = true;
                break;
            }
        }
        const std::string validation_layer = "VK_LAYER_KHRONOS_validation";
        for (const vk::LayerProperties &layer : vk::enumerateInstanceLayerProperties()) {
            if (std::string(layer.layerName.data()) == validation_layer) {
                has_validation_layer = true;
                break;
            }
        }

        std::vector<const char *> instance_layers;
        if (has_debug_extension && has_validation_layer) {
            LOG_INFO("Enabling vulkan validation layers (has a performance impact but allows better error messages)");
            instance_extensions.push_back(debug_extension.c_str());
            instance_layers.push_back(validation_layer.c_str());
        }

        vk::InstanceCreateInfo instance_info{
            .pApplicationInfo = &app_info
        };
        instance_info.setPEnabledLayerNames(instance_layers);
        instance_info.setPEnabledExtensionNames(instance_extensions);

        instance = vk::createInstance(instance_info);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);

        if (has_debug_extension && has_validation_layer) {
            vk::DebugUtilsMessengerCreateInfoEXT debug_info{
                .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
                    | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
                .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                    | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
                .pfnUserCallback = debug_callback
            };
            debug_messenger = instance.createDebugUtilsMessengerEXT(debug_info);
        }
    }

    // Create Surface
    if (!screen_renderer.create(window))
        return false;

    // Select Physical Device
    {
        std::vector<vk::PhysicalDevice> physical_devices = instance.enumeratePhysicalDevices();

        if (gpu_idx != 0) {
            // force choose the gpu
            physical_device = physical_devices[gpu_idx - 1];
        } else {
            // choose a suitable gpu
            for (const auto &device : physical_devices) {
                if (device_is_compatible(device)) {
                    physical_device = device;

                    // if it is an integrated gpu, try to find a discrete one
                    if (device.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
                        break;
                }
            }
        }

        physical_device_properties = physical_device.getProperties();
        physical_device_features = physical_device.getFeatures();
        physical_device_memory = physical_device.getMemoryProperties();
        physical_device_queue_families = physical_device.getQueueFamilyProperties();

        if (!physical_device) {
            LOG_ERROR("Failed to select Vulkan physical device.");
            return false;
        }

        LOG_INFO("Vulkan device: {}", physical_device_properties.deviceName);
        LOG_INFO("Driver version: {}", get_driver_version(physical_device_properties.vendorID, physical_device_properties.driverVersion));
    }

    bool support_dedicated_allocations = false;
    // Create Device
    {
        std::vector<vk::DeviceQueueCreateInfo> queue_infos;
        std::vector<std::vector<float>> queue_priorities;
        if (!select_queues(*this, queue_infos, queue_priorities)) {
            LOG_ERROR("Failed to select proper Vulkan queues. This is likely a bug.");
            return false;
        }

        if (!physical_device.getSurfaceSupportKHR(
                general_family_index, screen_renderer.surface)) {
            LOG_ERROR("Failed to select a Vulkan queue that supports presentation. This is likely a bug.");
            return false;
        }

        // use these features (because they are used by the vita GPU) if they are available
        vk::PhysicalDeviceFeatures enabled_features{
            .fillModeNonSolid = physical_device_features.fillModeNonSolid,
            .wideLines = physical_device_features.wideLines,
            .samplerAnisotropy = physical_device_features.samplerAnisotropy,
        };

        // look for optional extensions
        std::vector<const char *> device_extensions(required_device_extensions);
        bool temp_bool;
        bool support_global_priority = false;
        bool support_buffer_device_address = false;
        bool support_standard_layout = false;
        const std::map<std::string, bool *> optional_extensions = {
            { VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME, &temp_bool },
            // can be used by vma to improve performance
            { VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME, &support_dedicated_allocations },
            // used to tell the driver this application is high priority
            { VK_EXT_GLOBAL_PRIORITY_EXTENSION_NAME, &support_global_priority },
            // can be used to specify which format will be used by mutable images
            { VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME, &surface_cache.support_image_format_specifier },
            { VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME, &temp_bool },
            // can host memory directly be used for gxm memory
            { VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME, &features.support_memory_mapping },
            // also needed for reading mapped memory in the shader
            { VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, &support_buffer_device_address },
            // needed for uniform uvec2 arrays not to take twice the size
            { VK_KHR_UNIFORM_BUFFER_STANDARD_LAYOUT_EXTENSION_NAME, &support_standard_layout }
        };

        for (const vk::ExtensionProperties &ext : physical_device.enumerateDeviceExtensionProperties()) {
            auto it = optional_extensions.find(ext.extensionName.data());
            if (it != optional_extensions.end()) {
                // this extension is available on the GPU
                *it->second = true;
                device_extensions.push_back(it->first.c_str());
            }
        }

        if (support_buffer_device_address) {
            auto features = physical_device.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceBufferAddressFeaturesEXT>();
            support_buffer_device_address &= static_cast<bool>(features.get<vk::PhysicalDeviceBufferAddressFeaturesEXT>().bufferDeviceAddress);
        }
        features.support_memory_mapping &= support_buffer_device_address;

        if (support_standard_layout) {
            auto features = physical_device.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceUniformBufferStandardLayoutFeatures>();
            support_standard_layout &= static_cast<bool>(features.get<vk::PhysicalDeviceUniformBufferStandardLayoutFeatures>().uniformBufferStandardLayout);
        }
        features.support_memory_mapping &= support_standard_layout;

        if (features.support_memory_mapping) {
            // disable memory mapping on GPUs with an alignment requirement higher than 4096 (should only
            // concern a few intel iGPUs)
            auto props = physical_device.getProperties2KHR<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceExternalMemoryHostPropertiesEXT>();
            features.support_memory_mapping &= static_cast<bool>(props.get<vk::PhysicalDeviceExternalMemoryHostPropertiesEXT>().minImportedHostPointerAlignment <= 4096);
        }

#ifdef __APPLE__
        // we need to make a copy of the vertex buffer for moltenvk, so disable memory mapping
        features.support_memory_mapping = false;
#endif

        if (features.support_memory_mapping)
            LOG_INFO("Memory mapping is enabled");

        if (physical_device_properties.vendorID == 4318) {
            // Nvidia does not allow us to set the device priority higher than normal
            // no need to remove the priority extension
            support_global_priority = false;
        }
        // this is an emulator, tell the system it should have a high priority
        const vk::DeviceQueueGlobalPriorityCreateInfoEXT queue_priority{
            .globalPriority = vk::QueueGlobalPriorityEXT::eHigh
        };
        if (support_global_priority) {
            // add queue_priority to each queue creation info
            for (auto &queue_info : queue_infos) {
                queue_info.pNext = &queue_priority;
            }
        }

        // We use subpass input to get something similar to direct fragcolor access (there is no difference for the shader)
        features.direct_fragcolor = true;

        vk::StructureChain<vk::DeviceCreateInfo, vk::PhysicalDeviceBufferAddressFeaturesEXT, vk::PhysicalDeviceUniformBufferStandardLayoutFeatures> device_info{
            vk::DeviceCreateInfo{
                .pEnabledFeatures = &enabled_features },
            vk::PhysicalDeviceBufferAddressFeaturesEXT{
                .bufferDeviceAddress = VK_TRUE },
            vk::PhysicalDeviceUniformBufferStandardLayoutFeatures{
                .uniformBufferStandardLayout = VK_TRUE }
        };
        device_info.get().setQueueCreateInfos(queue_infos);
        device_info.get().setPEnabledExtensionNames(device_extensions);

        if (!features.support_memory_mapping) {
            device_info.unlink<vk::PhysicalDeviceBufferAddressFeaturesEXT>();
            device_info.unlink<vk::PhysicalDeviceUniformBufferStandardLayoutFeatures>();
        }

        try {
            device = physical_device.createDevice(device_info.get());
        } catch (vk::NotPermittedKHRError) {
            // according to the vk spec, when using a priority higher than medium
            // we can get this error (although I think it will only possibly happen
            // for realtime priority)
            for (auto &queue_info : queue_infos) {
                queue_info.pNext = nullptr;
            }
            device = physical_device.createDevice(device_info.get());
        }
        VULKAN_HPP_DEFAULT_DISPATCHER.init(device);
    }

    // Get Queues
    general_queue = device.getQueue(general_family_index, 0);
    transfer_queue = device.getQueue(transfer_family_index, 0);

    // Create Command Pools
    {
        vk::CommandPoolCreateInfo general_pool_info{
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer, // Flags
            .queueFamilyIndex = general_family_index // Queue Family Index
        };

        vk::CommandPoolCreateInfo transfer_pool_info{
            .flags = vk::CommandPoolCreateFlagBits::eTransient, // Flags
            .queueFamilyIndex = transfer_family_index // Queue Family Index
        };

        general_command_pool = device.createCommandPool(general_pool_info);
        transfer_command_pool = device.createCommandPool(transfer_pool_info);
    }

    // Allocate Memory for Images and Buffers
    {
        vma::VulkanFunctions vulkan_functions{
            .vkGetInstanceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr,
            .vkGetDeviceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr
        };

        vma::AllocatorCreateInfo allocator_info = {
            .physicalDevice = physical_device,
            .device = device,
            .pVulkanFunctions = &vulkan_functions,
            .instance = instance,
            .vulkanApiVersion = VK_API_VERSION_1_0,
        };

        if (support_dedicated_allocations)
            allocator_info.flags |= vma::AllocatorCreateFlagBits::eKhrDedicatedAllocation;

        allocator = vma::createAllocator(allocator_info);
    }

    // create the default image and buffer
    {
        default_buffer = vkutil::Buffer(allocator, KiB(4));
        default_buffer.init_buffer(vk::BufferUsageFlagBits::eVertexBuffer);

        // create the default image, it must be cleared then transitioned
        default_image = vkutil::Image(allocator, 1, 1, vk::Format::eR8G8B8A8Unorm);

        default_image.init_image(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst);
        vk::CommandBuffer cmd_buffer = vkutil::create_single_time_command(device, general_command_pool);
        default_image.transition_to(cmd_buffer, vkutil::ImageLayout::TransferDst);
        // make it white
        vk::ClearColorValue white{
            .float32 = std::array<float, 4>{ 1.0f, 1.0f, 1.0f, 1.0f }
        };
        cmd_buffer.clearColorImage(default_image.image, vk::ImageLayout::eTransferDstOptimal, white, vkutil::color_subresource_range);
        default_image.transition_to(cmd_buffer, vkutil::ImageLayout::SampledImage);
        vkutil::end_single_time_command(device, general_queue, general_command_pool, cmd_buffer);

        // create the default sampler
        vk::SamplerCreateInfo sampler_info{
            .magFilter = vk::Filter::eLinear,
            .minFilter = vk::Filter::eLinear,
            .mipmapMode = vk::SamplerMipmapMode::eLinear,
            .addressModeU = vk::SamplerAddressMode::eRepeat,
            .addressModeV = vk::SamplerAddressMode::eRepeat,
            .addressModeW = vk::SamplerAddressMode::eRepeat,
            .minLod = 0.0f,
            .maxLod = 0.0f,
        };
        default_image.sampler = device.createSampler(sampler_info);
    }

    if (!screen_renderer.setup(base_path))
        return false;

    pipeline_cache.init();
    texture_cache.backend = &current_backend;
    texture::init(texture_cache, false);

    return true;
}

void VKState::cleanup() {
    device.waitIdle();

    screen_renderer.cleanup();

    allocator.destroy();

    device.destroy(general_command_pool);
    device.destroy(transfer_command_pool);

    device.destroy();
    instance.destroy();
}

void VKState::render_frame(const SceFVector2 &viewport_pos, const SceFVector2 &viewport_size, const DisplayState &display,
    const GxmState &gxm, MemState &mem) {
    // we are displaying this frame, wait for a new one
    should_display = false;

    if (!display.frame.base)
        return;

    if (!screen_renderer.acquire_swapchain_image())
        return;

    // Check if the surface exists
    std::array<float, 4> uvs = { 0.0f, 0.0f, 1.0f, 1.0f };
    SceFVector2 texture_size;

    vk::ImageLayout layout = vk::ImageLayout::eGeneral;
    vk::ImageView surface_handle = surface_cache.sourcing_color_surface_for_presentation(
        display.frame.base, display.frame.image_size.x, display.frame.image_size.y, display.frame.pitch, uvs, this->res_multiplier, texture_size);

    if (!surface_handle) {
        vkutil::Image &vita_surface = screen_renderer.vita_surface[screen_renderer.swapchain_image_idx];
        if (display.frame.image_size.x != vita_surface.width || display.frame.image_size.y != vita_surface.height) {
            // re-create the image
            vita_surface.destroy();
            vita_surface = vkutil::Image(allocator, display.frame.image_size.x, display.frame.image_size.y, vk::Format::eR8G8B8A8Unorm);
            vita_surface.init_image(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst);
        }

        // copy surface to staging buffer
        const vk::DeviceSize texture_data_size = display.frame.pitch * display.frame.image_size.y * 4;
        memcpy(screen_renderer.vita_surface_staging_info.pMappedData, display.frame.base.get(mem), texture_data_size);

        // copy staging buffer to image
        auto &cmd_buffer = screen_renderer.current_cmd_buffer;
        vita_surface.transition_to_discard(cmd_buffer, vkutil::ImageLayout::TransferDst);
        vk::BufferImageCopy region{
            .bufferOffset = 0,
            .bufferRowLength = display.frame.pitch,
            .bufferImageHeight = static_cast<uint32_t>(display.frame.image_size.y),
            .imageSubresource = vkutil::color_subresource_layer,
            .imageOffset = { 0, 0, 0 },
            .imageExtent = { static_cast<uint32_t>(display.frame.image_size.x), static_cast<uint32_t>(display.frame.image_size.y), 1 }
        };
        cmd_buffer.copyBufferToImage(screen_renderer.vita_surface_staging, vita_surface.image, vk::ImageLayout::eTransferDstOptimal, region);

        vita_surface.transition_to(cmd_buffer, vkutil::ImageLayout::SampledImage);

        surface_handle = vita_surface.view;
        texture_size = { static_cast<float>(display.frame.image_size.x), static_cast<float>(display.frame.image_size.y) };
        layout = vk::ImageLayout::eShaderReadOnlyOptimal;
    }

    screen_renderer.render(surface_handle, layout, uvs, texture_size);
}

void VKState::swap_window(SDL_Window *window) {
    screen_renderer.swap_window();

    // look once a frame if we need to save the pipeline cache
    const auto time_s = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    if (time_s >= pipeline_cache.next_pipeline_cache_save) {
        pipeline_cache.save_pipeline_cache();

        pipeline_cache.next_pipeline_cache_save = std::numeric_limits<uint64_t>::max();
    }
}

void VKState::set_fxaa(bool enable_fxaa) {
    screen_renderer.enable_fxaa = enable_fxaa;
}

bool VKState::map_memory(void *address, uint32_t size) {
    assert(features.support_memory_mapping);
    // the adress should be 4K aligned
    assert(((uint64_t)address & 4095) == 0);

    auto host_mem_props = device.getMemoryHostPointerPropertiesEXT(vk::ExternalMemoryHandleTypeFlagBits::eHostAllocationEXT, address);
    uint32_t host_mem_types = host_mem_props.memoryTypeBits;
    assert(host_mem_types != 0);

    uint32_t mapped_memory_type = 0;
    while (host_mem_types != 0) {
        // try to find a cached memory type
        mapped_memory_type = std::countr_zero(host_mem_types);
        host_mem_types -= (1 << mapped_memory_type);

        if (physical_device_memory.memoryTypes[mapped_memory_type].propertyFlags & vk::MemoryPropertyFlagBits::eHostCached)
            break;
    }

    vk::StructureChain<vk::MemoryAllocateInfo, vk::ImportMemoryHostPointerInfoEXT> alloc_info{
        vk::MemoryAllocateInfo{
            .allocationSize = size,
            .memoryTypeIndex = mapped_memory_type },
        vk::ImportMemoryHostPointerInfoEXT{
            .handleType = vk::ExternalMemoryHandleTypeFlagBits::eHostAllocationEXT,
            .pHostPointer = address }
    };
    const vk::DeviceMemory device_memory = device.allocateMemory(alloc_info.get());

    vk::StructureChain<vk::BufferCreateInfo, vk::ExternalMemoryBufferCreateInfoKHR> buffer_info{
        vk::BufferCreateInfo{
            .size = size,
            .usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddressEXT,
            .sharingMode = vk::SharingMode::eExclusive },
        vk::ExternalMemoryBufferCreateInfoKHR{
            .handleTypes = vk::ExternalMemoryHandleTypeFlagBits::eHostAllocationEXT }
    };
    const vk::Buffer mapped_buffer = device.createBuffer(buffer_info.get());
    device.bindBufferMemory(mapped_buffer, device_memory, 0);

    vk::BufferDeviceAddressInfoKHR address_info{
        .buffer = mapped_buffer
    };
    const uint64_t buffer_address = device.getBufferAddress(address_info);

    mapped_memories[std::bit_cast<uint64_t>(address)] = { address, size, device_memory, mapped_buffer, buffer_address };

    return true;
}

void VKState::unmap_memory(void *address) {
    assert(features.support_memory_mapping);

    auto ite = mapped_memories.find(std::bit_cast<uint64_t>(address));
    if (ite == mapped_memories.end()) {
        LOG_CRITICAL("Could not find mapped memory to erase");
        return;
    }

    // we need to wait in case the buffer is being used
    device.waitIdle();

    // deferred destory it instead
    device.destroyBuffer(ite->second.buffer);
    device.freeMemory(ite->second.memory);
    mapped_memories.erase(ite);
}

std::tuple<vk::Buffer, uint32_t> VKState::get_matching_mapping(const void *address) {
    uint64_t address_value = std::bit_cast<uint64_t>(address);
    auto mapped_memory = mapped_memories.lower_bound(address_value);
    if (mapped_memory == mapped_memories.end()
        || mapped_memory->first + mapped_memory->second.size < address_value) {
        LOG_ERROR("Could not find matching mapped buffer for vertex stream");
        return { nullptr, 0 };
    }

    return std::make_tuple(mapped_memory->second.buffer, static_cast<uint32_t>(address_value - mapped_memory->first));
}

uint64_t VKState::get_matching_device_address(const void *address) {
    uint64_t address_value = std::bit_cast<uint64_t>(address);
    auto mapped_memory = mapped_memories.lower_bound(address_value);
    if (mapped_memory == mapped_memories.end()
        || mapped_memory->first + mapped_memory->second.size < address_value) {
        LOG_ERROR("Could not find matching mapped buffer for vertex stream");
        return 0;
    }

    return mapped_memory->second.buffer_address + address_value - mapped_memory->first;
}

int VKState::get_max_anisotropic_filtering() {
    return static_cast<int>(physical_device_properties.limits.maxSamplerAnisotropy);
}

void VKState::set_anisotropic_filtering(int anisotropic_filtering) {
    texture_cache.anisotropic_filtering = anisotropic_filtering;
}

std::vector<std::string> VKState::get_gpu_list() {
    const std::vector<vk::PhysicalDevice> gpus = instance.enumeratePhysicalDevices();

    std::vector<std::string> gpu_list;
    // First value is always automatic
    gpu_list.push_back("Automatic");
    for (const vk::PhysicalDevice gpu : gpus)
        gpu_list.push_back(std::string(gpu.getProperties().deviceName.data()));

    return gpu_list;
}

void VKState::precompile_shader(const ShadersHash &hash) {
    Sha256Hash empty_hash{};
    if (hash.vert != empty_hash) {
        pipeline_cache.precompile_shader(hash.vert);
    }
    if (hash.frag != empty_hash) {
        pipeline_cache.precompile_shader(hash.frag);
    }

    programs_count_pre_compiled++;
    LOG_INFO("Program Compiled {}/{}", programs_count_pre_compiled, shaders_cache_hashs.size());
}

void VKState::preclose_action() {
    // make sure we are in a game
    if (!title_id[0])
        return;

    pipeline_cache.save_pipeline_cache();
}
} // namespace renderer::vulkan
