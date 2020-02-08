#include "VkBuffer.hpp"

static uint32_t find_memory_type(vk::PhysicalDevice physical_device, uint32_t type_filter, vk::MemoryPropertyFlags properties) {
    // Get available memory types
    vk::PhysicalDeviceMemoryProperties const device_properties = physical_device.getMemoryProperties();
    // Find a matching one
    for (uint32_t i = 0; i < device_properties.memoryTypeCount; ++i) {
        // If the filter matches the memory type, return the index of the memory type
        if (type_filter & (1 << i) && 
            (device_properties.memoryTypes[i].propertyFlags & properties)) { // Also check if memory properties match
            return i;
        }
    }

    assert(false && "Failed to find suitable memory type\n");
    return -1;
}

Buffer::Buffer(vk::PhysicalDevice physical_device, vk::Device device, vk::DeviceSize size, 
              vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties)
    : physical_device(physical_device), device(device) {

    vk::BufferCreateInfo info;
    info.size = size;
    info.usage = usage;
    info.sharingMode = vk::SharingMode::eExclusive;

    buffer = device.createBuffer(info);

    vk::MemoryRequirements const requirements = device.getBufferMemoryRequirements(buffer);
    vk::MemoryAllocateInfo alloc_info;
    alloc_info.allocationSize = requirements.size;
    alloc_info.memoryTypeIndex = find_memory_type(physical_device, requirements.memoryTypeBits, properties);

    memory = device.allocateMemory(alloc_info);
    device.bindBufferMemory(buffer, memory, 0);
}

Buffer::Buffer(Buffer&& rhs) {
    buffer = rhs.buffer;
    memory = rhs.memory;

    rhs.buffer = nullptr;
    rhs.memory = nullptr;
}

Buffer& Buffer::operator=(Buffer&& rhs) {
    if (this != &rhs) {
        destroy();
        buffer = rhs.buffer;
        memory = rhs.memory;

        rhs.buffer = nullptr;
        rhs.memory = nullptr;
    }
    return *this;
}

Buffer::~Buffer() {
    destroy();
}

vk::Buffer Buffer::handle() {
    return buffer;
}

vk::DeviceMemory Buffer::memory_handle() {
    return memory;
}

void Buffer::destroy() {
    if (buffer) {
        device.destroyBuffer(buffer);
    }

    if (memory) {
        device.freeMemory(memory);
    }
}

void copy_buffers(Buffer& src, Buffer& dest, vk::DeviceSize size, vk::CommandPool cmd_pool, vk::Queue queue) {
    vk::CommandBufferAllocateInfo cmdbuf_info;
    cmdbuf_info.commandBufferCount = 1;
    cmdbuf_info.level = vk::CommandBufferLevel::ePrimary;
    cmdbuf_info.commandPool = cmd_pool;

    vk::CommandBuffer cmd_buffer = src.device.allocateCommandBuffers(cmdbuf_info)[0];

    vk::CommandBufferBeginInfo begin_info;
    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    cmd_buffer.begin(begin_info);

    vk::BufferCopy copy_info;
    copy_info.size = size;

    cmd_buffer.copyBuffer(src.buffer, dest.buffer, copy_info);

    cmd_buffer.end();

    vk::SubmitInfo submit_info;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buffer;

    queue.submit(submit_info, nullptr);
    // Wait for the buffer copy to complete
    queue.waitIdle();

    // Free resources
    src.device.freeCommandBuffers(cmd_pool, cmd_buffer);
}