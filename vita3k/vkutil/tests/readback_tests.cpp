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

#define VMA_IMPLEMENTATION
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mem/functions.h>
#include <mem/state.h>
#include <stdexcept>
#include <thread>
#include <vkutil/readback.h>
#if defined(__APPLE__)
#include <mach/mach.h>
#include <mach/mach_vm.h>
#endif
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

static void require(bool pass, const char *message) {
    if (!pass)
        throw std::runtime_error(message);
}
static std::atomic<uint32_t> validation_errors{ 0 };
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT *data, void *) {
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        ++validation_errors;
        std::cerr << "VALIDATION: " << data->pMessage << std::endl;
    }
    return VK_FALSE;
}

int main() try {
    const char *loader_path = std::getenv("VITA3K_TEST_VULKAN_LOADER");
    vk::detail::DynamicLoader loader(loader_path ? loader_path : "libvulkan.dylib");
    const auto get = loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
    require(get, "Vulkan loader missing");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(get);
    const char *extensions[] = { "VK_KHR_portability_enumeration", "VK_EXT_debug_utils" };
    const auto available = vk::enumerateInstanceLayerProperties();
    const bool validation = std::any_of(available.begin(), available.end(), [](auto &v) { return std::strcmp(v.layerName, "VK_LAYER_KHRONOS_validation") == 0; });
    const char *layer = "VK_LAYER_KHRONOS_validation";
    vk::ApplicationInfo app{ .pApplicationName = "Vita3K readback fixture", .apiVersion = VK_API_VERSION_1_1 };
    vk::InstanceCreateInfo ci{ .flags = vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR, .pApplicationInfo = &app, .enabledLayerCount = validation ? 1U : 0U, .ppEnabledLayerNames = validation ? &layer : nullptr, .enabledExtensionCount = 2, .ppEnabledExtensionNames = extensions };
    auto instance = vk::createInstanceUnique(ci);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);
    auto messenger = instance->createDebugUtilsMessengerEXTUnique({ .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
        .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
        .pfnUserCallback = reinterpret_cast<vk::PFN_DebugUtilsMessengerCallbackEXT>(debug_callback) });
    const auto devices = instance->enumeratePhysicalDevices();
    require(!devices.empty(), "No Vulkan GPU");
    const auto physical = devices[0];
    const auto families = physical.getQueueFamilyProperties();
    uint32_t family = 0;
    while (family < families.size() && !(families[family].queueFlags & vk::QueueFlagBits::eGraphics))
        ++family;
    require(family < families.size(), "No graphics queue");
    const float priority = 1;
    const vk::DeviceQueueCreateInfo qci{ .queueFamilyIndex = family, .queueCount = 1, .pQueuePriorities = &priority };
    const auto devext = physical.enumerateDeviceExtensionProperties();
    const char *portability = "VK_KHR_portability_subset";
    const bool subset = std::any_of(devext.begin(), devext.end(), [&](auto &e) { return std::strcmp(e.extensionName, portability) == 0; });
    auto device = physical.createDeviceUnique({ .queueCreateInfoCount = 1, .pQueueCreateInfos = &qci, .enabledExtensionCount = subset ? 1U : 0U, .ppEnabledExtensionNames = subset ? &portability : nullptr });
    VULKAN_HPP_DEFAULT_DISPATCHER.init(*device);
    const auto queue = device->getQueue(family, 0);
    std::mutex queue_mutex;
    auto pool = device->createCommandPoolUnique({ .flags = vk::CommandPoolCreateFlagBits::eTransient, .queueFamilyIndex = family });
    const vma::VulkanFunctions functions{ .vkGetInstanceProcAddr = get, .vkGetDeviceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr };
    const auto allocator = vma::createAllocator({ .physicalDevice = physical, .device = *device, .pVulkanFunctions = &functions, .instance = *instance, .vulkanApiVersion = VK_API_VERSION_1_0 });
    vkutil::init(allocator);
    MemState mem;
    require(init(mem, false), "Guest memory allocation failed");
    const auto base = alloc(mem, 128 * 1024, "readback fixture");
    require(base, "Guest region allocation failed");
    std::memset(mem.memory.get() + base, 0xa5, 128 * 1024);
    uint32_t cases = 0;
    for (const auto format : { vk::Format::eR5G6B5UnormPack16, vk::Format::eR8G8B8A8Unorm }) {
        for (const float scale : { 1.0f, 1.5f, 2.0f }) {
            const uint32_t width = 96, height = 54, sw = uint32_t(width * scale), sh = uint32_t(height * scale);
            vkutil::Image image(sw + 8, sh + 8, format);
            image.init_image(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst);
            const vk::AttachmentDescription attachment{ .format = format, .samples = vk::SampleCountFlagBits::e1, .loadOp = vk::AttachmentLoadOp::eClear, .storeOp = vk::AttachmentStoreOp::eStore, .stencilLoadOp = vk::AttachmentLoadOp::eDontCare, .stencilStoreOp = vk::AttachmentStoreOp::eDontCare, .initialLayout = vk::ImageLayout::eColorAttachmentOptimal, .finalLayout = vk::ImageLayout::eColorAttachmentOptimal };
            const vk::AttachmentReference reference{ .attachment = 0, .layout = vk::ImageLayout::eColorAttachmentOptimal };
            const vk::SubpassDescription subpass{ .pipelineBindPoint = vk::PipelineBindPoint::eGraphics, .colorAttachmentCount = 1, .pColorAttachments = &reference };
            auto renderpass = device->createRenderPassUnique({ .attachmentCount = 1, .pAttachments = &attachment, .subpassCount = 1, .pSubpasses = &subpass });
            auto framebuffer = device->createFramebufferUnique({ .renderPass = *renderpass, .attachmentCount = 1, .pAttachments = &image.view, .width = image.width, .height = image.height, .layers = 1 });
            const uint32_t bpp = format == vk::Format::eR5G6B5UnormPack16 ? 2 : 4;
            for (uint32_t pass = 0; pass < 2; ++pass) {
                auto command = device->allocateCommandBuffersUnique({ .commandPool = *pool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1 });
                const auto cmd = *command[0];
                cmd.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
                image.transition_to(cmd, vkutil::ImageLayout::ColorAttachment);
                const vk::ClearValue black{ .color = { .float32 = std::array<float, 4>{ 0, 0, 0, 1 } } };
                cmd.beginRenderPass({ .renderPass = *renderpass, .framebuffer = *framebuffer, .renderArea = { { 0, 0 }, { image.width, image.height } }, .clearValueCount = 1, .pClearValues = &black }, vk::SubpassContents::eInline);
                for (uint32_t quadrant = 0; quadrant < 4; ++quadrant) {
                    const uint32_t q = (quadrant + pass) % 4;
                    const std::array<float, 4> color = { float(q == 0 || q == 3), float(q == 1 || q == 3), float(q == 2 || q == 3), 1 };
                    const vk::ClearAttachment clear{ .aspectMask = vk::ImageAspectFlagBits::eColor, .colorAttachment = 0, .clearValue = { .color = { .float32 = color } } };
                    const uint32_t x = (quadrant & 1) ? sw / 2 : 0, y = (quadrant & 2) ? sh / 2 : 0;
                    const vk::ClearRect rectangle{ .rect = { { int32_t(x), int32_t(y) }, { (quadrant & 1) ? sw - sw / 2 : sw / 2, (quadrant & 2) ? sh - sh / 2 : sh / 2 } }, .baseArrayLayer = 0, .layerCount = 1 };
                    cmd.clearAttachments(clear, rectangle);
                }
                cmd.endRenderPass();
                image.transition_to(cmd, vkutil::ImageLayout::ColorAttachmentReadWrite);
                cmd.end();
                const vk::SubmitInfo submit{ .commandBufferCount = 1, .pCommandBuffers = &cmd };
                queue.submit(submit); // deliberately do not wait before production readback
                vkutil::Image crop;
                crop.destroy_on_deletion = false;
                crop.image = image.image;
                crop.format = format;
                crop.layout = image.layout;
                crop.width = sw;
                crop.height = sh;
                std::vector<uint8_t> raw, converted;
                require(vkutil::read_image_to_host(physical, *device, queue, queue_mutex, *pool, allocator, crop, width, height, raw), "GPU readback failed");
                require(raw.size() == width * height * bpp, "Readback extent mismatch");
                const vk::ComponentMapping identity{ vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, bpp == 2 ? vk::ComponentSwizzle::eOne : vk::ComponentSwizzle::eA };
                require(vkutil::convert_readback_pixels(format, identity, bpp, raw, converted), "Format conversion failed");
                for (uint32_t y = 0; y < height; ++y)
                    for (uint32_t x = 0; x < width; ++x) {
                        const uint32_t q = (((x >= width / 2) ? 1 : 0) + ((y >= height / 2) ? 2 : 0) + pass) % 4;
                        const size_t at = (size_t(y) * width + x) * bpp;
                        if (bpp == 2) {
                            const uint16_t expected[] = { 0xf800, 0x07e0, 0x001f, 0xffff };
                            uint16_t actual;
                            std::memcpy(&actual, converted.data() + at, 2);
                            require(actual == expected[q], "Packed RGB565/color/orientation mismatch");
                        } else {
                            const uint8_t expected[4][4] = { { 255, 0, 0, 255 }, { 0, 255, 0, 255 }, { 0, 0, 255, 255 }, { 255, 255, 255, 255 } };
                            require(std::memcmp(converted.data() + at, expected[q], 4) == 0, "RGBA color/orientation mismatch");
                        }
                    }
                const uint32_t row = width * bpp, stride = row + 16;
                std::memset(mem.memory.get() + base, 0xa5, 128 * 1024);
                std::atomic<uint32_t> callbacks{ 0 };
                const auto protected_address = base;
                require(add_protect(mem, protected_address, mem.host_page_size * 2, MemPerm::None, [&](Address, bool) {++callbacks;return true; }), "Protect setup failed");
                const auto segments_before = mem.protect_tree.size();
                require(write_gpu_readback_rows(mem, base, converted.data(), row, height, stride), "Protected guest commit failed");
                require(callbacks == 0 && mem.protect_tree.size() == segments_before, "GPU write damaged CPU invalidation metadata");
                require(handle_access_violation(mem, mem.memory.get() + protected_address, false), "Protected segment not retained");
                require(callbacks > 0, "Protection callback lost");
                for (uint32_t y = 0; y < height; ++y) {
                    require(std::memcmp(mem.memory.get() + base + y * stride, converted.data() + y * row, row) == 0, "Guest row mismatch");
                    for (uint32_t pad = row; pad < stride; ++pad)
                        require(mem.memory.get()[base + y * stride + pad] == 0xa5, "Padding overwritten");
                }
                require(mem.memory.get()[base + height * stride] == 0xa5, "Adjacent bytes overwritten");
                auto unchanged = converted;
                require(!write_gpu_readback_rows(mem, 0, converted.data(), row, height, stride), "Null guest accepted");
                require(!write_gpu_readback_rows(mem, base, converted.data(), row, height, row - 1), "Short stride accepted");
                std::vector<uint8_t> output = { 9 };
                require(!vkutil::convert_readback_pixels(vk::Format::eUndefined, identity, bpp, raw, output) && output == std::vector<uint8_t>{ 9 }, "Unsupported format mutated output");
                if (bpp == 4) {
                    require(vkutil::convert_readback_pixels(format, { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eOne }, 3, raw, output) && output.size() == width * height * 3, "RGB24 conversion failed");
                    for (size_t z = 0; z < output.size() / 3; ++z)
                        require(std::memcmp(output.data() + z * 3, raw.data() + z * 4, 3) == 0, "RGB24 packing mismatch");
                }
                ++cases;
            }
        }
    }
#if defined(__APPLE__)
    require(mem.host_page_size >= 16384, "Mixed-permission fixture needs macOS16KiB host page");
    std::atomic<uint32_t> mixed_callbacks{ 0 };
    require(add_protect(mem, base, 4096, MemPerm::ReadOnly, [&](Address, bool) {++mixed_callbacks;return true; }), "Mixed RO setup");
    require(add_protect(mem, base + 8192, 4096, MemPerm::None, [&](Address, bool) {++mixed_callbacks;return true; }), "Mixed None setup");
    const uint8_t sixteen[16] = { 1, 2, 3, 4 };
    require(write_gpu_readback_rows(mem, base + 4096, sixteen, 16, 1, 16), "Mixed permission commit");
    mach_vm_address_t address = reinterpret_cast<mach_vm_address_t>(mem.memory.get() + base);
    mach_vm_size_t region_size = 0;
    vm_region_basic_info_data_64_t region{};
    mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object = 0;
    require(mach_vm_region(mach_task_self(), &address, &region_size, VM_REGION_BASIC_INFO_64,
                reinterpret_cast<vm_region_info_t>(&region), &count, &object)
            == KERN_SUCCESS,
        "Query host permission");
    require(region.protection == VM_PROT_NONE, "Read trap on shared16KiB page weakened");
    require(mixed_callbacks == 0 && mem.protect_tree.size() == 2, "Mixed metadata changed");
    require(handle_access_violation(mem, mem.memory.get() + base + 8192, false), "Mixed None callback");
    require(handle_access_violation(mem, mem.memory.get() + base, true), "Mixed RO callback");
    require(mixed_callbacks == 2, "Mixed callbacks lost");
    require(std::memcmp(mem.memory.get() + base + 4096, sixteen, 16) == 0, "Mixed commit bytes");
    mixed_callbacks = 0;
    require(add_protect(mem, base, 5 * 4096, MemPerm::ReadOnly, [&](Address, bool) {++mixed_callbacks;return true; }), "Long RO setup");
    require(add_protect(mem, base + 6 * 4096, 4096, MemPerm::None, [&](Address, bool) {++mixed_callbacks;return true; }), "Neighbour None setup");
    require(write_gpu_readback_rows(mem, base + 4096, sixteen, 16, 1, 16), "Long segment commit");
    address = reinterpret_cast<mach_vm_address_t>(mem.memory.get() + base + 6 * 4096);
    count = VM_REGION_BASIC_INFO_COUNT_64;
    require(mach_vm_region(mach_task_self(), &address, &region_size, VM_REGION_BASIC_INFO_64,
                reinterpret_cast<vm_region_info_t>(&region), &count, &object)
            == KERN_SUCCESS,
        "Query neighbour protection");
    require(region.protection == VM_PROT_NONE, "Long segment commit weakened a neighbouring host page");
    require(mixed_callbacks == 0 && mem.protect_tree.size() == 2, "Long segment metadata changed");
    require(handle_access_violation(mem, mem.memory.get() + base + 6 * 4096, false), "Neighbour None callback");
    require(handle_access_violation(mem, mem.memory.get() + base, true), "Long RO callback");
    require(mixed_callbacks == 2, "Long segment callbacks lost");
#endif
    require(validation_errors == 0, "Vulkan validation error");
    std::cout << "{\"passed\":true,\"gpu_cases\":" << cases << ",\"validation_enabled\":" << (validation ? "true" : "false") << ",\"validation_errors\":" << validation_errors << ",\"device\":\"" << physical.getProperties().deviceName << "\"}" << std::endl;
    queue.waitIdle();
    free(mem, base);
    vkutil::deinit();
    allocator.destroy();
    return 0;
} catch (const std::exception &error) {
    std::cerr << error.what() << std::endl;
    return 1;
}
