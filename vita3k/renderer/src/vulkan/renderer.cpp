// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

#include <renderer/types.h>
#include <renderer/functions.h>

#include <config/version.h>
#include <features/features.h>

#include <renderer/vulkan/state.h>
#include <renderer/vulkan/functions.h>

#include <SDL.h>
#include <SDL_vulkan.h>

// Setting a default value for now.
// In the future, it might be a good idea to take the host's device memory into account.
constexpr static size_t private_allocation_size = MB(1);

// Some random number as value. It will likely be very different. There are probably SCE feilds for this, I will look later.
constexpr static uint32_t max_sets = 192;
constexpr static uint32_t max_buffers = 64;
constexpr static uint32_t max_images = 64;
constexpr static uint32_t max_samplers = 64;

constexpr static vk::Format screen_format = vk::Format::eB8G8R8A8Unorm;

const static FeatureState default_features = {
    .direct_pack_unpack_half = false,
    .pack_unpack_half_through_ext = false, // OpenGL Extension
    .support_shader_interlock = true,
    .support_texture_barrier = true,
    .direct_fragcolor = false, // OpenGL Extension
};

const static std::vector<const char *> instance_layers = {
#ifndef NDEBUG
    "VK_LAYER_LUNARG_standard_validation",
#endif
};

const static std::vector<const char *> instance_extensions = {
    "VK_KHR_surface",
#ifdef __APPLE__
    "VK_MVK_macos_surface",
#endif
#ifdef WIN32
    "VK_KHR_win32_surface",
#endif
};

const static std::vector<const char *> device_layers = {
    // Nothing yet.
};

const static std::vector<const char *> device_extensions = {
    "VK_KHR_swapchain",
};

const static vk::PhysicalDeviceFeatures required_features({
    // .vertexPipelineStoresAndAtomics = true
    // etc.
});

std::vector<vk::DescriptorPoolSize> descriptor_pool_sizes = {
    vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, max_buffers),
    vk::DescriptorPoolSize(vk::DescriptorType::eSampledImage, max_images),
    vk::DescriptorPoolSize(vk::DescriptorType::eSampler, max_samplers),
};

namespace renderer::vulkan {
static bool device_is_compatible(vk::PhysicalDeviceProperties &properties, vk::PhysicalDeviceFeatures &features) {
    // TODO: Do any required checks here. Should check against required_features.

    return true;
}

static bool select_queues(VulkanState &vulkan_state,
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

    for (size_t a = 0; a < vulkan_state.physical_device_queue_families.size(); a++) {
        const auto &queue_family = vulkan_state.physical_device_queue_families[a];

        // MoltenVK does not accept nullptr a pPriorities for some reason.
        std::vector<float> &priorities = queue_priorities.emplace_back(queue_family.queueCount, 1.0f);

        // Only one DeviceQueueCreateInfo should be created per family.
        if (!found_graphics && queue_family.queueFlags & vk::QueueFlagBits::eGraphics
            && queue_family.queueFlags & vk::QueueFlagBits::eTransfer) {
            queue_infos.emplace_back(
                 vk::DeviceQueueCreateFlagBits(), // No Flags
                 a, // Queue Family Index
                 queue_family.queueCount, // Queue Count
                 priorities.data() // Priorities
                 );
            vulkan_state.general_family_index = a;
            found_graphics = true;
        } else if (!found_transfer && queue_family.queueFlags & vk::QueueFlagBits::eTransfer) {
            queue_infos.emplace_back(
                vk::DeviceQueueCreateFlagBits(), // No Flags
                a, // Queue Family Index
                queue_family.queueCount, // Queue Count
                priorities.data() // Priorities
                );
            vulkan_state.transfer_family_index = a;
            found_transfer = true;
        }

        if (found_graphics && found_transfer)
            break;
    }

    return found_graphics && found_transfer;
}

static void select_memory(VulkanState &vulkan_state) {
    bool found_private = false;

    for (uint32_t a = 0; a < vulkan_state.physical_device_memory.memoryTypeCount; a++) {
        const auto &type = vulkan_state.physical_device_memory.memoryTypes[a];
        const auto &heap = vulkan_state.physical_device_memory.memoryHeaps[a];

        if (type.propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal && heap.size >= private_allocation_size) {
            vulkan_state.private_memory_index = a;
            found_private = true;
        }

        if (found_private)
            break;
    }

    assert(found_private);
}

vk::CommandBuffer create_command_buffer(VulkanState &state, CommandType type) {
    vk::CommandBuffer buffer;

    switch (type) {
        case CommandType::General: {
            vk::CommandBufferAllocateInfo command_buffer_info(
                state.general_command_pool, // Command Pool
                vk::CommandBufferLevel::ePrimary, // Level
                1 // Count
                );

            state.device.allocateCommandBuffers(&command_buffer_info, &buffer);
            break;
        }
        case CommandType::Transfer: {
            vk::CommandBufferAllocateInfo command_buffer_info(
                state.transfer_command_pool, // Command Pool
                vk::CommandBufferLevel::ePrimary, // Level
                1 // Count
                );

            state.device.allocateCommandBuffers(&command_buffer_info, &buffer);
            break;
        }
    }

    assert(buffer);

    return buffer;
}

void free_command_buffer(VulkanState &state, CommandType type, vk::CommandBuffer buffer) {
    vk::CommandPool pool;

    switch (type) {
        case CommandType::General:
            pool = state.general_command_pool;
            break;
        case CommandType::Transfer:
            pool = state.transfer_command_pool;
            break;
    }

    state.device.freeCommandBuffers(pool, 1, &buffer);
}

void submit_command_buffer(VulkanState &state, CommandType type, vk::CommandBuffer buffer) {
    vk::Queue queue;

    switch (type) {
        case CommandType::General:
            queue = state.general_queues[state.general_queue_last % state.general_queues.size()];
            state.general_queue_last++;
            break;
        case CommandType::Transfer:
            queue = state.transfer_queues[state.transfer_queue_last & state.transfer_queues.size()];
            state.transfer_queue_last++;
            break;
    }

    vk::SubmitInfo submit_info(
        0, nullptr, // Wait Semaphores
        nullptr, // Destination Stage Mask
        1, &buffer, // Command Buffers
        0, nullptr // Signal Semaphores
        );

    queue.submit(1, &submit_info, vk::Fence());
}

// TODO: Replace asserts for vulkan methods and VULKAN_CHECK so the method fails.
bool create(WindowPtr window, std::unique_ptr<renderer::State> &state) {
    auto &vulkan_state = dynamic_cast<VulkanState &>(*state);

    // Create Instance
    {
        vk::ApplicationInfo app_info(
            app_name, // App Name
            0, // App Version
            org_name, // Engine Name, using org instead.
            0, // Engine Version
            VK_MAKE_VERSION(1, 0, 0) // Lowest possible.
        );

        vk::InstanceCreateInfo instance_info(
            vk::InstanceCreateFlags(), // No Flags
            &app_info, // App Info
            instance_layers.size(), instance_layers.data(), // No Layers
            instance_extensions.size(), instance_extensions.data() // No Extensions
        );

        vulkan_state.instance = vk::createInstance(instance_info, nullptr);
        assert(vulkan_state.instance);
    }

    // Select Physical Device
    {
        std::vector<vk::PhysicalDevice> physical_devices = vulkan_state.instance.enumeratePhysicalDevices();

        for (const auto &device : physical_devices) {
            vk::PhysicalDeviceProperties properties = device.getProperties();
            vk::PhysicalDeviceFeatures features = device.getFeatures();
            if (device_is_compatible(properties, features)) {
                vulkan_state.physical_device = device;
                vulkan_state.physical_device_properties = properties;
                vulkan_state.physical_device_features = features;
                vulkan_state.physical_device_memory = device.getMemoryProperties();
                vulkan_state.physical_device_queue_families = device.getQueueFamilyProperties();
                break;
            }
        }

        assert(vulkan_state.physical_device);
    }

    // Create Device
    {
        std::vector<vk::DeviceQueueCreateInfo> queue_infos;
        std::vector<std::vector<float>> queue_priorities;
        assert(select_queues(vulkan_state, queue_infos, queue_priorities));

        vk::DeviceCreateInfo device_info(
            vk::DeviceCreateFlags(), // No Flags
            queue_infos.size(), queue_infos.data(), // No Queues
            device_layers.size(), device_layers.data(), // No Layers
            device_extensions.size(), device_extensions.data(), // No Extensions
            &required_features
            );

        vulkan_state.device = vulkan_state.physical_device.createDevice(device_info, nullptr);
        assert(vulkan_state.device);
    }

    // Get Queues
    for (uint32_t a = 0; a < vulkan_state.physical_device_queue_families[vulkan_state.general_family_index].queueCount; a++) {
        vulkan_state.general_queues.push_back(
            vulkan_state.device.getQueue(vulkan_state.general_family_index, a));
    }

    for (uint32_t a = 0; a < vulkan_state.physical_device_queue_families[vulkan_state.transfer_family_index].queueCount; a++) {
        vulkan_state.transfer_queues.push_back(
            vulkan_state.device.getQueue(vulkan_state.transfer_family_index, a));
    }

    // Create Command Pools
    {
        vk::CommandPoolCreateInfo general_pool_info(
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer, // Flags
            vulkan_state.general_family_index // Queue Family Index
        );

        vk::CommandPoolCreateInfo transfer_pool_info(
            vk::CommandPoolCreateFlagBits::eTransient, // Flags
            vulkan_state.transfer_family_index // Queue Family Index
        );

        vulkan_state.general_command_pool = vulkan_state.device.createCommandPool(general_pool_info, nullptr);
        assert(vulkan_state.general_command_pool);
        vulkan_state.transfer_command_pool = vulkan_state.device.createCommandPool(transfer_pool_info, nullptr);
        assert(vulkan_state.transfer_command_pool);

        vulkan_state.gui_command_buffer = create_command_buffer(vulkan_state, CommandType::General);
        vulkan_state.general_command_buffer = create_command_buffer(vulkan_state, CommandType::General);
    }

    // Allocate Memory for Images and Buffers
    {
        vk::MemoryAllocateInfo private_info(
            private_allocation_size, // Size
            vulkan_state.private_memory_index // Index
        );

        vulkan_state.private_memory = vulkan_state.device.allocateMemory(private_info, nullptr);
        assert(vulkan_state.private_memory);
    }

    // Create Descriptor Pool
    {
        vk::DescriptorPoolCreateInfo descriptor_pool_info(
            vk::DescriptorPoolCreateFlags(), // No Flags
            max_sets, // Maximum Sets
            descriptor_pool_sizes.size(), descriptor_pool_sizes.data() // Descriptor Pool
        );

        vulkan_state.descriptor_pool = vulkan_state.device.createDescriptorPool(descriptor_pool_info, nullptr);
        assert(vulkan_state.descriptor_pool);
    }

    // Create Surface and Swapchain
    {
        VkSurfaceKHR surface;
        assert(SDL_Vulkan_CreateSurface(window.get(), vulkan_state.instance, &surface));
        vulkan_state.surface = vk::SurfaceKHR(surface);

        assert(vulkan_state.physical_device.getSurfaceSupportKHR(vulkan_state.general_family_index,
                                                                 vulkan_state.surface));

        vk::SwapchainCreateInfoKHR swapchain_info(
            vk::SwapchainCreateFlagsKHR(), // No Flags
            vulkan_state.surface, // Surface
            2, // Double Buffering
            screen_format, // Image Format, BGRA is supported by MoltenVK
            vk::ColorSpaceKHR::eSrgbNonlinear, // Color Space
            vk::Extent2D(DEFAULT_RES_WIDTH, DEFAULT_RES_HEIGHT), // Image Extent
            1, // Image Array Length
            vk::ImageUsageFlagBits::eColorAttachment, // Image Usage, consider VK_IMAGE_USAGE_STORAGE_BIT?
            vk::SharingMode::eExclusive,
            0, nullptr, // Unused when sharing mode is exclusive
            vk::SurfaceTransformFlagBitsKHR::eIdentity, // Transform
            vk::CompositeAlphaFlagBitsKHR::eOpaque, // Alpha
            vk::PresentModeKHR::eFifo, // Present Mode, FIFO and Immediate are supported on MoltenVK. Would've chosen Mailbox otherwise.
            true, // Clipping
            vk::SwapchainKHR() // No old swapchain.
        );

        vulkan_state.swapchain = vulkan_state.device.createSwapchainKHR(swapchain_info, nullptr);
        assert(vulkan_state.swapchain);
    }

    // Get Swapchain Images
    vulkan_state.swapchain_images = vulkan_state.device.getSwapchainImagesKHR(vulkan_state.swapchain);

    for (uint32_t a = 0; a < 2 /*vulkan_state.swapchain_images.size()*/; a++) {
        const auto &image = vulkan_state.swapchain_images[a];
        vk::ImageViewCreateInfo view_info(
            vk::ImageViewCreateFlags(), // No Flags
            image, // Image
            vk::ImageViewType::e2D, // Image View Type
            screen_format, // Format
            vk::ComponentMapping(), // Default Component Mapping
            vk::ImageSubresourceRange(
                vk::ImageAspectFlagBits::eColor,
                0, // Mipmap Level
                1, // Level Count
                0, // Base Array Index
                1 // Layer Count
                )
            );

        vk::ImageView view = vulkan_state.device.createImageView(view_info, nullptr);
        assert(view);

        vulkan_state.swapchain_views[a] = view;
    }

    return true;
}

// Not used, but Vulkan requires some special shutdown. I want to get it written down.
void close(std::unique_ptr<renderer::State> &state) {
    auto &vulkan_state = reinterpret_cast<VulkanState &>(*state);

    // Mainly this line. We could make this VulkanState's destructor?
    vulkan_state.device.waitIdle();

    vulkan_state.device.destroy(vulkan_state.swapchain);
    vulkan_state.instance.destroy(vulkan_state.surface);

    vulkan_state.device.destroy(vulkan_state.descriptor_pool);

    vulkan_state.device.free(vulkan_state.private_memory);

    free_command_buffer(vulkan_state, CommandType::General, vulkan_state.gui_command_buffer);
    free_command_buffer(vulkan_state, CommandType::General, vulkan_state.general_command_buffer);

    vulkan_state.device.destroy(vulkan_state.general_command_pool);
    vulkan_state.device.destroy(vulkan_state.transfer_command_pool);

    vulkan_state.device.destroy();
    vulkan_state.instance.destroy();
}
}
