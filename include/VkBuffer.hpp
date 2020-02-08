#ifndef VK_BUFFER_HPP_
#define VK_BUFFER_HPP_

#include <vulkan/vulkan.hpp>

class Buffer {
public:
    Buffer() = default;
    Buffer(vk::PhysicalDevice physical_device, vk::Device device, vk::DeviceSize size, 
           vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties);

    Buffer(Buffer const&) = delete;
    Buffer(Buffer&& rhs);

    Buffer& operator=(Buffer const&) = delete;
    Buffer& operator=(Buffer&& rhs);

    ~Buffer();

    vk::Buffer handle();
    vk::DeviceMemory memory_handle();

    void destroy();

    friend void copy_buffers(Buffer& src, Buffer& dest, vk::DeviceSize size, vk::CommandPool cmd_pool, vk::Queue queue);

private:   
    vk::PhysicalDevice physical_device;
    vk::Device device;

    vk::Buffer buffer;
    vk::DeviceMemory memory;
};


#endif