#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

#undef max
#undef min

#include <iostream>
#include <limits>
#include <optional>
#include <vector>

static GLFWwindow* init_glfw(size_t w, size_t h, const char* title) {
    glfwInit();

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
    if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    vk::Extent2D actual_extent = vk::Extent2D(window_w, window_h);

    actual_extent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actual_extent.width));
    actual_extent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actual_extent.height));

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
    }

    ~VulkanApp() {
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
        }
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
    vk::Format swapchain_format;
    vk::Extent2D swapchain_extent;

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
        std::vector<std::uint32_t> queue_families = { indices.graphics_family.value(), indices.present_family.value() };
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
        std::uint32_t image_count = swap_chain_support.capabilities.minImageCount + 1;
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
        std::uint32_t indices_array[] = { indices.graphics_family.value(), indices.present_family.value() };
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
};


int main() {
    VulkanApp app(1280, 720, "Vulkan");
    app.run();

    return 0;
}