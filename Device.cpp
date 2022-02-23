
#include "Allocator.h"

#include "Command.h"

#include "dynalo/dynalo.hpp"

namespace mz
{
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

    VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures = {
        .sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
        .dynamicRendering = 1,
    };

    VkPhysicalDeviceVulkan12Features vk12featuers = {
        .sType             = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext             = &dynamicRenderingFeatures,
        .timelineSemaphore = VK_TRUE,
    };

    VkPhysicalDeviceFeatures2 features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &vk12featuers,
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

    MZ_VULKAN_ASSERT_SUCCESS(vkCreateDevice(PhysicalDevice, &info, 0, &handle));
    vkl_load_device_functions(handle, this);

    ImmAllocator = std::make_shared<VulkanAllocator>(this);
    ImmCmdPool   = std::make_shared<CommandPool>(this, QueueFamily);
}

VulkanDevice::~VulkanDevice()
{
    for (auto& [id, glob] : Globals)
    {
        glob.Free();
    }

    ImmAllocator.reset();
    ImmCmdPool.reset();
    DestroyDevice(0);
}

VulkanContext::VulkanContext()
    : lib(dynalo::open("vulkan-1.dll"))
{

    MZ_VULKAN_ASSERT_SUCCESS(vkl_init(dynalo::get_function<decltype(vkGetInstanceProcAddr)>((dynalo::native::handle)lib, "vkGetInstanceProcAddr")));
    u32 count;

    // MZ_VULKAN_ASSERT_SUCCESS(vkEnumerateInstanceLayerProperties(&count, 0));
    // std::vector<VkLayerProperties> props(count);
    // MZ_VULKAN_ASSERT_SUCCESS(vkEnumerateInstanceLayerProperties(&count, props.data()));

    // for (auto& prop : props) {
    //   printf("%s\n", prop.layerName);
    // }

    VkApplicationInfo app = {
        .sType      = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .apiVersion = VK_API_VERSION_1_3,
    };

    std::vector<const char*> layers = {
        "VK_LAYER_KHRONOS_validation",
    };

    std::vector<const char*> extensions = {
        "VK_KHR_surface",
        "VK_KHR_win32_surface",
        "VK_KHR_external_memory_capabilities",
    };

    VkInstanceCreateInfo info = {
        .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo        = &app,
        .enabledLayerCount       = (u32)layers.size(),
        .ppEnabledLayerNames     = layers.data(),
        .enabledExtensionCount   = (u32)extensions.size(),
        .ppEnabledExtensionNames = extensions.data(),
    };

    MZ_VULKAN_ASSERT_SUCCESS(vkCreateInstance(&info, 0, &Instance));

    vkl_load_instance_functions(Instance);

    MZ_VULKAN_ASSERT_SUCCESS(vkEnumeratePhysicalDevices(Instance, &count, 0));

    std::vector<VkPhysicalDevice> pdevices(count);
    Devices.reserve(count);

    MZ_VULKAN_ASSERT_SUCCESS(vkEnumeratePhysicalDevices(Instance, &count, pdevices.data()));

    std::vector<const char*> deviceExtensions = {
        "VK_KHR_swapchain",
        "VK_KHR_external_semaphore_win32",
        "VK_KHR_external_memory_win32",
        "VK_EXT_external_memory_host",
        "VK_KHR_dynamic_rendering",
        "VK_KHR_timeline_semaphore",
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
} // namespace mz