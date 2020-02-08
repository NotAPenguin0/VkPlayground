#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#undef max
#undef min

#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <vector>

#include "VkBuffer.hpp"

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;

    static vk::VertexInputBindingDescription input_binding_description() {
        vk::VertexInputBindingDescription description;
        description.binding = 0;
        description.stride = sizeof(Vertex);
        // Like glVertexAttribDivisor
        description.inputRate = vk::VertexInputRate::eVertex;

        return description;
    }

    static std::array<vk::VertexInputAttributeDescription, 2> attribute_descriptions() {
        std::array<vk::VertexInputAttributeDescription, 2> attributes;

        // Position

        // This binding is the same as the binding above
        attributes[0].binding = 0;
        attributes[0].location = 0;
        attributes[0].format = vk::Format::eR32G32B32Sfloat;
        attributes[0].offset = offsetof(Vertex, pos);
        // Color
        attributes[1].binding = 0;
        attributes[1].location = 1;
        attributes[1].format = vk::Format::eR32G32B32Sfloat;
        attributes[1].offset = offsetof(Vertex, color);

        return attributes;
    }
};

constexpr std::array<Vertex, 4> vertices = {
    Vertex{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
    Vertex{{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
    Vertex{{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    Vertex{{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}}
};

constexpr std::array<uint32_t, 6> indices = {
    0, 1, 2, 2, 3, 0
};

constexpr size_t max_frames_in_flight = 2;

static std::string read_file(std::string_view fname) {
    std::ifstream file(fname.data(), std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

static GLFWwindow* init_glfw(size_t w, size_t h, const char* title) {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(w, h, title, nullptr, nullptr);
    return window;
}

struct ExtensionsInfo {
    std::vector<char const*> names;
};

static ExtensionsInfo get_required_instance_extensions() {
    ExtensionsInfo info;

    uint32_t glfw_count;
    
    // Get extensions required for GLFW to work
    char const** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_count);
    // Construct extension names vector from a pair of iterators
    info.names = std::vector<char const*>(glfw_extensions, glfw_extensions + glfw_count);

    // Add own required extensions

    // Extension to install message callback for validation layers
    info.names.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    return info;
}

static ExtensionsInfo get_required_device_extensions() {
    ExtensionsInfo info;
    info.names.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    return info;
}

// Vulkan debug callback. WARNING: The debug callback api is not wrapped by Vulkan.hpp, so the C types must be used
static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    [[maybe_unused]] VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT type,
    [[maybe_unused]] VkDebugUtilsMessengerCallbackDataEXT const* callback_data,
    [[maybe_unused]] void* user_data) {

    // Only log messages with severity 'warning' or above
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::cerr << "Validation layers: ";
        std::cerr << callback_data->pMessage << std::endl;
    }

    return VK_FALSE;
}

struct QueueFamilyIndices {
    std::optional<uint32_t> graphics_family;
    std::optional<uint32_t> present_family;
};

static QueueFamilyIndices find_queue_families(vk::PhysicalDevice device, vk::SurfaceKHR surface) {
    QueueFamilyIndices indices;

    std::vector<vk::QueueFamilyProperties> queue_families = device.getQueueFamilyProperties();

    size_t index = 0;
    for (auto const& family : queue_families) {
        if (family.queueFlags & vk::QueueFlagBits::eGraphics) {
            indices.graphics_family = index;
        } 

        // check if the device has present queue support
        if (device.getSurfaceSupportKHR(index, surface)) {
            indices.present_family = index;
        }

        ++index;
    }

    return indices;
}

struct SwapChainSupportDetails {
    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> present_modes;
};

static SwapChainSupportDetails get_swapchain_support_details(vk::PhysicalDevice device, vk::SurfaceKHR surface) {
    SwapChainSupportDetails details;

    details.capabilities = device.getSurfaceCapabilitiesKHR(surface);
    details.formats = device.getSurfaceFormatsKHR(surface);
    details.present_modes = device.getSurfacePresentModesKHR(surface);

    return details;
}

static vk::SurfaceFormatKHR choose_swap_surface_format(std::vector<vk::SurfaceFormatKHR> const& formats) {
    for (auto const& format : formats) {
        // This is a nice format, we want to use it
        if (format.format == vk::Format::eB8G8R8A8Unorm && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            return format;
        }
    }
    // If our preferred format was not available, return the first one in the list
    return formats[0];
}

static vk::PresentModeKHR choose_swap_present_mode(std::vector<vk::PresentModeKHR> const& present_modes) {
    // Prefer Mailbox mode (can be used for triple buffering)
    for (auto const& mode : present_modes) {
        if (mode == vk::PresentModeKHR::eMailbox) {
            return mode;
        }
    }

    // This one is guaranteed to be available, so choose it as the default mode
    return vk::PresentModeKHR::eFifo;
}

static vk::Extent2D choose_swap_extent(vk::SurfaceCapabilitiesKHR const& capabilities, size_t window_w, size_t window_h) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    vk::Extent2D actual_extent = vk::Extent2D(window_w, window_h);

    actual_extent.width = std::max(capabilities.minImageExtent.width, 
                                   std::min(capabilities.maxImageExtent.width, actual_extent.width));
    actual_extent.height = std::max(capabilities.minImageExtent.height, 
                                    std::min(capabilities.maxImageExtent.height, actual_extent.height));

    return actual_extent;
}

static int physical_device_score(vk::PhysicalDevice device, vk::SurfaceKHR surface) {
    vk::PhysicalDeviceProperties properties = device.getProperties();
    vk::PhysicalDeviceFeatures features = device.getFeatures();

    int score = 0;

    // Prefer a dedicated GPU over integrated ones
    if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
        score += 1000;
    }

    // Max texture quality contributes as well
    score += properties.limits.maxImageDimension2D;

    // Check for available queue families
    QueueFamilyIndices queue_families = find_queue_families(device, surface);
    // If the device does not have a graphics family, do not select it
    if (!queue_families.graphics_family.has_value()) {
        return 0;
    }

    // Require present queue support
    if (!queue_families.present_family.has_value()) {
        return 0;
    }

    // Check for required extensions
    ExtensionsInfo required_extensions = get_required_device_extensions();
    std::vector<vk::ExtensionProperties> device_extensions = device.enumerateDeviceExtensionProperties();
    for (auto const& extension : required_extensions.names) {
        bool found = false;
        // Check if requested extension is available
        for (auto const& properties : device_extensions) {
            if (std::strcmp(properties.extensionName, extension)) {
                found = true;
            }
        }
        // These extensions are required for the appication to work
        if (!found) {
            return 0;
        }
    }

    // Check swapchain capabilities
    SwapChainSupportDetails swapchain_details = get_swapchain_support_details(device, surface);
    // Require at least one format and one present mode
    if (swapchain_details.formats.empty() || swapchain_details.present_modes.empty()) {
        return 0;
    }

    return score;
}


class VulkanApp {
public:
    VulkanApp(size_t width, size_t height, const char* title) : window_w(width), window_h(height) {
        window = init_glfw(width, height, title);
        get_available_instance_extensions();
        create_instance();
        // Create dispatcher for dynamically dispatching some functions
        dynamic_dispatcher = { instance, vkGetInstanceProcAddr };
        // The debug messenger needs access to an initialized vkInstance
        create_debug_messenger();
        create_surface();
        pick_physical_device();
        create_logical_device();
        create_swapchain();
        create_image_views();
        create_render_pass();
        create_graphics_pipeline();
        create_framebuffers();
        create_command_pool();
        create_vertex_buffer();
        create_index_buffer();
        create_command_buffers();
        create_sync_objects();
    }

    ~VulkanApp() {
        index_buffer.destroy();
        vertex_buffer.destroy();
        for (auto& sync_set : sync_objects) {
            device.destroySemaphore(sync_set.image_available);
            device.destroySemaphore(sync_set.render_finished);
            device.destroyFence(sync_set.frame_fence);
        }

        device.destroyCommandPool(command_pool);
        for (auto const& framebuf : swapchain_framebuffers) {
            device.destroyFramebuffer(framebuf);
        }

        for (auto const& img_view : swapchain_image_views) {
            device.destroyImageView(img_view);
        }
        device.destroyPipeline(graphics_pipeline);
        device.destroyRenderPass(render_pass);
        device.destroyPipelineLayout(pipeline_layout);
        device.destroySwapchainKHR(swapchain);
        device.destroy();
        instance.destroyDebugUtilsMessengerEXT(debug_messenger, nullptr, dynamic_dispatcher);
        instance.destroySurfaceKHR(surface);
        instance.destroy();
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    void run() {
        while(!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            render_frame();
            current_frame = (current_frame + 1) % max_frames_in_flight;
        }
        
        // Wait until everything is done before starting to deallocate stuff
        device.waitIdle();
    }

private:
    size_t window_w, window_h;
    GLFWwindow* window;

    std::vector<vk::ExtensionProperties> extensions;
    vk::Instance instance;
    vk::DispatchLoaderDynamic dynamic_dispatcher;
    vk::DebugUtilsMessengerEXT debug_messenger;

    vk::PhysicalDevice physical_device;
    vk::Device device;
    vk::Queue graphics_queue;
    vk::Queue present_queue;

    vk::SurfaceKHR surface;
    vk::SwapchainKHR swapchain;

    std::vector<vk::Image> swapchain_images;
    std::vector<vk::ImageView> swapchain_image_views;
    vk::Format swapchain_format;
    vk::Extent2D swapchain_extent;

    vk::PipelineLayout pipeline_layout;
    vk::RenderPass render_pass;
    vk::Pipeline graphics_pipeline;

    std::vector<vk::Framebuffer> swapchain_framebuffers;

    vk::CommandPool command_pool;
    std::vector<vk::CommandBuffer> command_buffers;

    size_t current_frame = 0;

    // Synchronization
    struct SyncObjects {
        vk::Semaphore image_available;
        vk::Semaphore render_finished;
        vk::Fence frame_fence;
    };

    std::vector<SyncObjects> sync_objects;
    std::vector<vk::Fence> images_in_flight;

    Buffer vertex_buffer;
    Buffer index_buffer;

    void get_available_instance_extensions() {
        extensions = vk::enumerateInstanceExtensionProperties();
        std::cout << "Available instance extensions: " << "\n";
        for (auto const& extension : extensions) {
            std::cout << extension.extensionName << "\n";
        }
    }

    void check_available_validation_layers(std::vector<char const*> const& layers) {
        std::vector<vk::LayerProperties> available_layers = vk::enumerateInstanceLayerProperties();

        for (auto const& layer : layers) {
            bool found = false;
            // Check if requested layer is available
            for (auto const& properties : available_layers) {
                if (std::strcmp(properties.layerName, layer)) {
                    found = true;
                }
            }
            if (!found) {
                std::cerr << "Validation layer " << layer << " is not available\n";
            }
        }
    }

    void create_instance() {
        // Mostly optional application info.
        // The only required field is apiVersion
        vk::ApplicationInfo app_info;
        app_info.pApplicationName = "Vulkan Testing App";
        app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.pEngineName = "No Engine";
        app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.apiVersion = VK_API_VERSION_1_2;

        // Create the vkInstance
        vk::InstanceCreateInfo instance_info;
        instance_info.pApplicationInfo = &app_info;

        // Get extensions required by glfw
        ExtensionsInfo extensions = get_required_instance_extensions();
        instance_info.enabledExtensionCount = extensions.names.size();
        instance_info.ppEnabledExtensionNames = extensions.names.data();

        // Enable validation layers
        std::vector<char const*> validation_layers = {
            "VK_LAYER_KHRONOS_validation"
        };
        // Check if requested layers are available
        check_available_validation_layers(validation_layers);
        instance_info.enabledLayerCount = validation_layers.size();
        instance_info.ppEnabledLayerNames = validation_layers.data();

        // Create the instance
        instance = vk::createInstance(instance_info);
    }

    void create_debug_messenger() {
        vk::DebugUtilsMessengerCreateInfoEXT info;

        // Specify message severity and message types to log
        info.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | 
                               vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                               vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
        info.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                           vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral;
        info.pfnUserCallback = &vk_debug_callback;  

        
        debug_messenger = instance.createDebugUtilsMessengerEXT(info, nullptr, dynamic_dispatcher);
    }

    void create_surface() {
        if (glfwCreateWindowSurface(instance, window, nullptr, reinterpret_cast<VkSurfaceKHR*>(&surface)) != VK_SUCCESS) {
            std::cerr << "Failed to create surface";
            return;
        }
    }

    void pick_physical_device() {
        std::vector<vk::PhysicalDevice> devices = instance.enumeratePhysicalDevices();
        int max_score = 0;
        for (auto device : devices) {
            // Find 'best' physical device
            int score = physical_device_score(device, surface);
            if (score > max_score) {
                max_score = score;
                physical_device = device;
            }
        }

        vk::PhysicalDeviceProperties properties = physical_device.getProperties();
        std::cout << "Picked physical device: " << properties.deviceName << "\n";

        if (!physical_device) {
            std::cerr << "No physical device found\n";
            return;
        }
    }

    void create_logical_device() {
        QueueFamilyIndices indices = find_queue_families(physical_device, surface);

        // Create a single graphics queue
        std::vector<vk::DeviceQueueCreateInfo> queue_infos;
        std::vector<uint32_t> queue_families = { indices.graphics_family.value(), indices.present_family.value() };
        float priority = 1.0f;
        for (auto family : queue_families) {
            vk::DeviceQueueCreateInfo info;
            info.queueFamilyIndex = family;
            info.queueCount = 1;
            // This should be an array if there is more than one queue
            info.pQueuePriorities = &priority;

            queue_infos.push_back(info);
        }

        // Enumerate features we want enabled. We leave this at the default for now
        vk::PhysicalDeviceFeatures features;

        // Create the actual device
        vk::DeviceCreateInfo device_info;
        device_info.pQueueCreateInfos = queue_infos.data();
        device_info.queueCreateInfoCount = queue_infos.size();
        device_info.pEnabledFeatures = &features;
        
        // List required extensions and enable them
        ExtensionsInfo required_extensions = get_required_device_extensions();
        device_info.ppEnabledExtensionNames = required_extensions.names.data();
        device_info.enabledExtensionCount = required_extensions.names.size();
        
        device = physical_device.createDevice(device_info);

        // Find the graphics queue. The second parameter is the index of the queue
        graphics_queue = device.getQueue(indices.graphics_family.value(), 0);
        present_queue = device.getQueue(indices.present_family.value(), 0);
    }

    void create_swapchain() {
        SwapChainSupportDetails swap_chain_support = get_swapchain_support_details(physical_device, surface);

        vk::SurfaceFormatKHR surface_format = choose_swap_surface_format(swap_chain_support.formats);
        vk::PresentModeKHR present_mode = choose_swap_present_mode(swap_chain_support.present_modes);
        vk::Extent2D extent = choose_swap_extent(swap_chain_support.capabilities, window_w, window_h);

        // + 1 because we want to avoid the driver stalling if we do not have enough images
        uint32_t image_count = swap_chain_support.capabilities.minImageCount + 1;
        // Make sure not to exceed the maximum amount of images. 0 means no limit.
        if (swap_chain_support.capabilities.maxImageCount > 0 &&
            swap_chain_support.capabilities.maxImageCount < image_count) {

            image_count = swap_chain_support.capabilities.maxImageCount;
        }

        // Create the actual swapchain
        vk::SwapchainCreateInfoKHR info;
        info.surface = surface;
        info.minImageCount = image_count;
        info.imageColorSpace = surface_format.colorSpace;
        info.imageFormat = surface_format.format;
        info.imageExtent = extent;
        // This is basically always 1
        info.imageArrayLayers = 1;
        info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

        QueueFamilyIndices indices = find_queue_families(physical_device, surface);
        uint32_t indices_array[] = { indices.graphics_family.value(), indices.present_family.value() };
        // If the graphics and present queue are different, we have to tell Vulkan that we are using the swapchain image
        // concurrently.
        if(indices.graphics_family.value() != indices.present_family.value()) {
            info.imageSharingMode = vk::SharingMode::eConcurrent;
            info.queueFamilyIndexCount = 2;
            info.pQueueFamilyIndices = indices_array;
        } else {
            info.imageSharingMode = vk::SharingMode::eExclusive;
        }

        // Don't apply a special transformation to swapchain images
        info.preTransform = swap_chain_support.capabilities.currentTransform;
        // Ignore alpha channel to disable blending with other windows
        info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        info.presentMode = present_mode;
        info.clipped = VK_TRUE;
        info.oldSwapchain = nullptr;

        swapchain = device.createSwapchainKHR(info);
        swapchain_extent = extent;
        swapchain_format = surface_format.format;
        swapchain_images = device.getSwapchainImagesKHR(swapchain);
    }

    void create_image_views() {
        swapchain_image_views.resize(swapchain_images.size());
        for (size_t i = 0; i < swapchain_image_views.size(); ++i) {
            vk::ImageViewCreateInfo info;
            info.image = swapchain_images[i];
            info.viewType = vk::ImageViewType::e2D;
            info.format = swapchain_format;
            // Swizzle components (this shit is pretty cool).
            // Note that Vulkan.hpp has these values as the default anyway, so it's not nessecary to specify them here
            info.components.r = vk::ComponentSwizzle::eIdentity;
            info.components.g = vk::ComponentSwizzle::eIdentity;
            info.components.b = vk::ComponentSwizzle::eIdentity;
            info.components.a = vk::ComponentSwizzle::eIdentity;
            // subresourceRange describes what to use the image for
            info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
            info.subresourceRange.baseMipLevel = 0;
            info.subresourceRange.levelCount = 1;
            info.subresourceRange.baseArrayLayer = 0;
            info.subresourceRange.layerCount = 1;
            swapchain_image_views[i] = device.createImageView(info);
        }
    }

    vk::ShaderModule create_shader_module(std::string const& code) {
        vk::ShaderModuleCreateInfo info;
        info.codeSize = code.size();
        info.pCode = reinterpret_cast<uint32_t const*>(code.data());
        return device.createShaderModule(info);
    }

    void create_render_pass() {
        vk::AttachmentDescription color_attachment;
        color_attachment.format = swapchain_format;
        color_attachment.loadOp = vk::AttachmentLoadOp::eClear;
        color_attachment.storeOp = vk::AttachmentStoreOp::eStore;
        // We don't care about what happens to the stencil attachment now
        color_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        color_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        
        color_attachment.initialLayout = vk::ImageLayout::eUndefined;
        color_attachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

        // Create a single subpass
        vk::AttachmentReference color_attachment_ref;
        color_attachment_ref.attachment = 0;
        color_attachment_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;

        vk::SubpassDescription subpass_info;
        subpass_info.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
        subpass_info.colorAttachmentCount = 1;
        subpass_info.pColorAttachments = &color_attachment_ref;

        vk::SubpassDependency dependency;
        // Create a dependency between the implicit "transform" step before our subpass and our own subpass
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        // Specify where the dependency happens
        dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;

        // Create the actual render pass
        vk::RenderPassCreateInfo render_pass_info;
        render_pass_info.attachmentCount = 1;
        render_pass_info.pAttachments = &color_attachment;
        render_pass_info.subpassCount = 1;
        render_pass_info.pSubpasses = &subpass_info;
        render_pass_info.dependencyCount = 1;
        render_pass_info.pDependencies = &dependency;

        render_pass = device.createRenderPass(render_pass_info);
    }

    void create_graphics_pipeline() {
        std::string vert_shader_code = read_file("shaders/shader.vert.spv");
        std::string frag_shader_code = read_file("shaders/shader.frag.spv");

        vk::ShaderModule vert_module = create_shader_module(vert_shader_code);
        vk::ShaderModule frag_module = create_shader_module(frag_shader_code);

        // For each shader stage, we need a vk::PipelineShaderStage
        vk::PipelineShaderStageCreateInfo vert_info;
        vert_info.stage = vk::ShaderStageFlagBits::eVertex;
        vert_info.module = vert_module;
        // Specify entry point for shader
        vert_info.pName = "main";

        vk::PipelineShaderStageCreateInfo frag_info;
        frag_info.stage = vk::ShaderStageFlagBits::eFragment;
        frag_info.module = frag_module;
        frag_info.pName = "main";

        vk::PipelineShaderStageCreateInfo shader_stages[] = { vert_info, frag_info };

        // Similar to OpenGL vao objects
        vk::PipelineVertexInputStateCreateInfo vertex_input_info;
        
        auto const binding_info = Vertex::input_binding_description();
        auto const attribute_info = Vertex::attribute_descriptions();

        vertex_input_info.vertexBindingDescriptionCount = 1;
        vertex_input_info.pVertexBindingDescriptions = &binding_info;
        vertex_input_info.vertexAttributeDescriptionCount = attribute_info.size();
        vertex_input_info.pVertexAttributeDescriptions = attribute_info.data();

        vk::PipelineInputAssemblyStateCreateInfo input_assembly_info;
        input_assembly_info.topology = vk::PrimitiveTopology::eTriangleList;
        input_assembly_info.primitiveRestartEnable = false;

        // Define viewport and scissor region
        vk::Viewport viewport;
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = swapchain_extent.width;
        viewport.height = swapchain_extent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 0.0f;
        vk::Rect2D scissor;
        scissor.offset = vk::Offset2D{};
        scissor.extent = swapchain_extent;

        vk::PipelineViewportStateCreateInfo viewport_info;
        viewport_info.viewportCount = 1;
        viewport_info.pViewports = &viewport;
        viewport_info.scissorCount = 1;
        viewport_info.pScissors = &scissor;

        // Rasterizer create info
        vk::PipelineRasterizationStateCreateInfo rasterization_info;
        rasterization_info.depthClampEnable = false;
        rasterization_info.rasterizerDiscardEnable = false;
        rasterization_info.polygonMode = vk::PolygonMode::eFill;
        rasterization_info.lineWidth = 1.0f;
        rasterization_info.cullMode = vk::CullModeFlagBits::eBack;
        rasterization_info.frontFace = vk::FrontFace::eClockwise;
        // This setting can be useful for shadow mapping. Requires additional values to be set.
        rasterization_info.depthBiasEnable = false;

        // Setup multisample state
        vk::PipelineMultisampleStateCreateInfo multisample_info;
        multisample_info.sampleShadingEnable = false;
        multisample_info.rasterizationSamples = vk::SampleCountFlagBits::e1;
        // If we were to enable multisampling, we'd need to set a few more settings here

        // Do not enable depth testing for now

        // Setup color blending mode
        vk::PipelineColorBlendAttachmentState color_blend_attachment;
        color_blend_attachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG 
                                        | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
        color_blend_attachment.blendEnable = false;
        // Since we disable blending, we can leave the other settings at the default

        vk::PipelineColorBlendStateCreateInfo color_blend_info;
        color_blend_info.logicOpEnable = false;
        color_blend_info.attachmentCount = 1;
        color_blend_info.pAttachments = &color_blend_attachment;

        // Set dynamic state
        vk::DynamicState dynamic_states[] = {
            vk::DynamicState::eViewport
        };

        vk::PipelineDynamicStateCreateInfo dynamic_state_info;
        dynamic_state_info.dynamicStateCount = 1;
        dynamic_state_info.pDynamicStates = dynamic_states;

        // The pipeline layout specifies uniforms
        vk::PipelineLayoutCreateInfo pipeline_layout_info;
        pipeline_layout = device.createPipelineLayout(pipeline_layout_info);

        // Create the actual graphics pipeline
        vk::GraphicsPipelineCreateInfo pipeline_info;
        pipeline_info.stageCount = 2;
        pipeline_info.pStages = shader_stages;
        pipeline_info.pVertexInputState = &vertex_input_info;
        pipeline_info.pInputAssemblyState = &input_assembly_info;
        pipeline_info.pViewportState = &viewport_info;
        pipeline_info.pRasterizationState = &rasterization_info;
        pipeline_info.pMultisampleState = &multisample_info;
        pipeline_info.pColorBlendState = &color_blend_info;
        pipeline_info.pDynamicState = nullptr; // Disable dynamic state for now
        // Setup layout and render pass
        pipeline_info.layout = pipeline_layout;
        pipeline_info.renderPass = render_pass;
        pipeline_info.subpass = 0;

        graphics_pipeline = device.createGraphicsPipeline(nullptr, pipeline_info);

        // Just like with OpenGL shaders, we're allowed to destroy the modules when 
        // we have finished linking them together
        device.destroyShaderModule(vert_module);
        device.destroyShaderModule(frag_module);
    }

    void create_framebuffers() {
        swapchain_framebuffers.resize(swapchain_image_views.size());
        for (size_t i = 0; i < swapchain_framebuffers.size(); ++i) {
            // We only have one attachment for this framebuffer
            vk::ImageView attachments[] = {
                swapchain_image_views[i]
            };

            vk::FramebufferCreateInfo framebuffer_info;
            framebuffer_info.renderPass = render_pass;
            framebuffer_info.attachmentCount = 1;
            framebuffer_info.pAttachments = attachments;
            framebuffer_info.width = swapchain_extent.width;
            framebuffer_info.height = swapchain_extent.height;
            framebuffer_info.layers = 1;

            swapchain_framebuffers[i] = device.createFramebuffer(framebuffer_info);
        }
    }

    void create_command_pool() {
        QueueFamilyIndices queue_families = find_queue_families(physical_device, surface);
        vk::CommandPoolCreateInfo info;
        info.queueFamilyIndex = queue_families.graphics_family.value();

        command_pool = device.createCommandPool(info);
    }

    void create_vertex_buffer() {
        vk::DeviceSize buffer_size = vertices.size() * sizeof(Vertex);
        // Create staging buffer for transfer
        Buffer staging_buffer(physical_device, device, buffer_size, vk::BufferUsageFlagBits::eTransferSrc, 
                              vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible);
        // Copy data to staging buffer
        void* data_ptr;
        data_ptr = device.mapMemory(staging_buffer.memory_handle(), 0, buffer_size);
        std::memcpy(data_ptr, &vertices[0], buffer_size);
        device.unmapMemory(staging_buffer.memory_handle());

        // Create vertex buffer in device local memory and copy contents
        vertex_buffer = Buffer(physical_device, device, buffer_size, 
                               vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, 
                               vk::MemoryPropertyFlagBits::eDeviceLocal);
        // Do the copy
        copy_buffers(staging_buffer, vertex_buffer, buffer_size, command_pool, graphics_queue);
        // Temporary staging buffer is destroyed by the Buffer destructor
    }

    void create_index_buffer() {
         vk::DeviceSize buffer_size = indices.size() * sizeof(uint32_t);
        // Create staging buffer for transfer
        Buffer staging_buffer(physical_device, device, buffer_size, vk::BufferUsageFlagBits::eTransferSrc, 
                              vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible);
        // Copy data to staging buffer
        void* data_ptr;
        data_ptr = device.mapMemory(staging_buffer.memory_handle(), 0, buffer_size);
        std::memcpy(data_ptr, &indices[0], buffer_size);
        device.unmapMemory(staging_buffer.memory_handle());

        // Create index buffer in device local memory and copy contents
        index_buffer = Buffer(physical_device, device, buffer_size, 
                              vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, 
                              vk::MemoryPropertyFlagBits::eDeviceLocal);
        // Do the copy
        copy_buffers(staging_buffer, index_buffer, buffer_size, command_pool, graphics_queue);
        // Temporary staging buffer is destroyed by the Buffer destructor
    }

    void create_command_buffers() {
        // Create the command buffers
        vk::CommandBufferAllocateInfo info;
        info.commandPool = command_pool;
        info.level = vk::CommandBufferLevel::ePrimary;
        info.commandBufferCount = swapchain_framebuffers.size();

        command_buffers = device.allocateCommandBuffers(info);

        // Record commands to the command buffers
        for (size_t i = 0; i < command_buffers.size(); ++i) {
            vk::CommandBuffer cmd_buffer = command_buffers[i];
            // We're going to leave these values at their defaults
            vk::CommandBufferBeginInfo begin_info;
            // Start command buffer
            cmd_buffer.begin(begin_info);
            // Start render pass
            vk::RenderPassBeginInfo render_pass_info;
            render_pass_info.renderPass = render_pass;
            render_pass_info.framebuffer = swapchain_framebuffers[i];
            render_pass_info.renderArea.offset = vk::Offset2D{0, 0};
            render_pass_info.renderArea.extent = swapchain_extent;
            // Specify clear color
            vk::ClearValue clear_color = vk::ClearColorValue(std::array<float, 4>{{0.0f, 0.0f, 0.0f, 1.0f}});
            render_pass_info.clearValueCount = 1;
            render_pass_info.pClearValues = &clear_color;
            // Render pass started
            cmd_buffer.beginRenderPass(render_pass_info, vk::SubpassContents::eInline);
            // Bind the graphics pipeline
            cmd_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphics_pipeline);
            // Bind the vertex and index buffer
            vk::DeviceSize offset = 0;
            cmd_buffer.bindVertexBuffers(0, vertex_buffer.handle(), offset);
            cmd_buffer.bindIndexBuffer(index_buffer.handle(), 0, vk::IndexType::eUint32);
            // Do the drawcall
            cmd_buffer.drawIndexed(indices.size(), 1, 0, 0, 0);
            // End command buffer
            cmd_buffer.endRenderPass();
            cmd_buffer.end();
        }
    }

    void create_sync_objects() {
        vk::SemaphoreCreateInfo info;
        vk::FenceCreateInfo fence_info;
        fence_info.flags = vk::FenceCreateFlagBits::eSignaled;
        sync_objects.resize(max_frames_in_flight);
        for (auto& sync_set : sync_objects) {
            sync_set.image_available = device.createSemaphore(info);
            sync_set.render_finished = device.createSemaphore(info);
            sync_set.frame_fence = device.createFence(fence_info);
        }

        images_in_flight.resize(swapchain_images.size(), nullptr);
    }

    void render_frame() {
        // Wait for an available spot in the in-flight frames array
        device.waitForFences(sync_objects[current_frame].frame_fence, true, std::numeric_limits<std::uint64_t>::max());


        // 1. Get image from swapchain for rendering
        // 2. Execute the correct command buffer to render to this image
        // 3. Send it back to the swapchain for presenting

        // Step 1: Aqcuire image from swapchain
        uint32_t image_index = device.acquireNextImageKHR(swapchain, std::numeric_limits<std::uint64_t>::max(), 
                                                               sync_objects[current_frame].image_available, nullptr).value;
        // Check if a previous frame is using this image
        if (images_in_flight[image_index]) {
            device.waitForFences(images_in_flight[image_index], true, std::numeric_limits<std::uint64_t>::max());
        }

        // Mark this image in use by the current frame
        images_in_flight[image_index] = sync_objects[current_frame].frame_fence;

        // Step 2: Submit command buffer
        vk::SubmitInfo submit_info;
        // These are the semaphores we want to wait for
        vk::Semaphore wait_semaphores[] = { sync_objects[current_frame].image_available };
        // At what stage we need to start waiting for the image. This means we can already start running the vertex shader
        // even if the image is not available yet.
        vk::PipelineStageFlags wait_stages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = wait_semaphores;
        submit_info.pWaitDstStageMask = wait_stages;

        // Specify which command buffers to submit
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command_buffers[image_index];
        
        // Specify the semaphores to signal when the operation is done
        vk::Semaphore signal_semaphores[] = { sync_objects[current_frame].render_finished };
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = signal_semaphores;
        
        // Reset the fence right before we actually need to use it
        device.resetFences(sync_objects[current_frame].frame_fence);

        // Submit the command buffer
        graphics_queue.submit(submit_info, sync_objects[current_frame].frame_fence);

        // Step 3: Present to the swapchain
        vk::PresentInfoKHR present_info;
        // Wait for the render_finished semaphore to signal before presenting
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = signal_semaphores;

        // Set the swapchain to present to
        vk::SwapchainKHR swapchains[] = { swapchain };
        present_info.swapchainCount = 1;
        present_info.pSwapchains = swapchains;
        present_info.pImageIndices = &image_index;

        // Present!
        present_queue.presentKHR(present_info);
    }
};

#include <thread>

int main() {
    glfwInit();
    VulkanApp app(1280, 720, "Vulkan");
    app.run();

    return 0;
}