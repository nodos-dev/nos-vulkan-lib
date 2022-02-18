
#include "Device.h"

#include "dynalo/dynalo.hpp"

VulkanDevice::VulkanDevice(VkInstance                      Instance,
                           VkPhysicalDevice                PhysicalDevice,
                           std::vector<const char*> const& layers,
                           std::vector<const char*> const& extensions)
    : Instance(Instance), PhysicalDevice(PhysicalDevice), QueueFamily(0)
{
    u32 count;
    vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &count, 0);

    std::vector<VkQueueFamilyProperties> props(count);

    vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &count, props.data());

    for (auto& prop : props)
    {
        if ((prop.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            (prop.queueFlags & VK_QUEUE_COMPUTE_BIT) &&
            (prop.queueFlags & VK_QUEUE_TRANSFER_BIT))
        {
            break;
        }
        QueueFamily++;
    }

    float prio = 1.f;

    VkDeviceQueueCreateInfo qinfo = {
        .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = QueueFamily,
        .queueCount       = 1,
        .pQueuePriorities = &prio,
    };

    VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures = {
        .sType                           = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
        .descriptorBindingPartiallyBound = VK_TRUE,
    };

    VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures = {
        .sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
        .pNext            = &indexingFeatures,
        .dynamicRendering = 1,
    };

    VkPhysicalDeviceFeatures2 features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &dynamicRenderingFeatures,
    };

    VkDeviceCreateInfo info = {
        .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext                   = &features,
        .queueCreateInfoCount    = 1,
        .pQueueCreateInfos       = &qinfo,
        .enabledLayerCount       = (u32)layers.size(),
        .ppEnabledLayerNames     = layers.data(),
        .enabledExtensionCount   = (u32)extensions.size(),
        .ppEnabledExtensionNames = extensions.data(),
    };

    CHECKRE(vkCreateDevice(PhysicalDevice, &info, 0, &handle));
    vkl_load_device_functions(handle, this);
}

VulkanDevice::~VulkanDevice()
{
    for (auto& [id, glob] : Globals)
    {
        glob.Free();
    }
    DestroyDevice(0);
}

VulkanContext::VulkanContext()
    : lib(dynalo::open("vulkan-1.dll"))
{

    CHECKRE(vkl_init(dynalo::get_function<decltype(vkGetInstanceProcAddr)>((dynalo::native::handle)lib, "vkGetInstanceProcAddr")));
    u32 count;

    // CHECKRE(vkEnumerateInstanceLayerProperties(&count, 0));
    // std::vector<VkLayerProperties> props(count);
    // CHECKRE(vkEnumerateInstanceLayerProperties(&count, props.data()));

    // for (auto& prop : props) {
    //   printf("%s\n", prop.layerName);
    // }

    VkApplicationInfo app = {
        .sType      = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .apiVersion = VK_API_VERSION_1_3,
    };

    std::vector<const char*> layers = {"VK_LAYER_KHRONOS_validation"};

    std::vector<const char*> extensions = {
        "VK_KHR_surface",
        "VK_KHR_win32_surface",
    };

    VkInstanceCreateInfo info = {
        .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo        = &app,
        .enabledLayerCount       = (u32)layers.size(),
        .ppEnabledLayerNames     = layers.data(),
        .enabledExtensionCount   = (u32)extensions.size(),
        .ppEnabledExtensionNames = extensions.data(),
    };

    CHECKRE(vkCreateInstance(&info, 0, &Instance));

    vkl_load_instance_functions(Instance);

    CHECKRE(vkEnumeratePhysicalDevices(Instance, &count, 0));

    std::vector<VkPhysicalDevice> pdevices(count);
    Devices.reserve(count);

    CHECKRE(vkEnumeratePhysicalDevices(Instance, &count, pdevices.data()));

    std::vector<const char*> deviceExtensions = {
        "VK_KHR_swapchain",
        "VK_KHR_external_semaphore_win32",
        "VK_KHR_external_memory_win32",
        "VK_EXT_external_memory_host",
        "VK_KHR_dynamic_rendering",
    };

    for (auto pdev : pdevices)
    {
        Devices.emplace_back(std::make_shared<VulkanDevice>(
            Instance, pdev, layers, deviceExtensions));
    }
}

VulkanContext::~VulkanContext()
{
    Devices.clear();
    vkDestroyInstance(Instance, 0);
    dynalo::close((dynalo::native::handle)lib);
}
