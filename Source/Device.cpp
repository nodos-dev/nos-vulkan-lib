
#include <Device.h>

#include <Allocator.h>

#include <Command.h>

#include <dynalo/dynalo.hpp>

static std::vector<const char*> layers = {
    "VK_LAYER_KHRONOS_validation",
    "VK_LAYER_KHRONOS_synchronization2",
};

static std::vector<const char*> extensions = {
    "VK_KHR_surface",
    "VK_KHR_win32_surface",
    "VK_KHR_external_memory_capabilities",
};

static std::vector<const char*> deviceExtensions = {
    "VK_KHR_swapchain",
    "VK_KHR_external_semaphore_win32",
    "VK_KHR_external_memory_win32",
    "VK_EXT_external_memory_host",
    "VK_KHR_synchronization2",
};

namespace mz::vk
{

Device::Device(VkInstance Instance, VkPhysicalDevice PhysicalDevice)
    : Instance(Instance), PhysicalDevice(PhysicalDevice)
{
    u32 count;

#ifndef NDEBUG
    MZ_VULKAN_ASSERT_SUCCESS(vkEnumerateDeviceExtensionProperties(PhysicalDevice, 0, &count, 0));
    std::vector<VkExtensionProperties> extensionProps(count);
    MZ_VULKAN_ASSERT_SUCCESS(vkEnumerateDeviceExtensionProperties(PhysicalDevice, 0, &count, extensionProps.data()));

    for (auto ext : deviceExtensions)
    {
        if (std::find_if(extensionProps.begin(), extensionProps.end(), [=](auto& prop) {
                return 0 == strcmp(ext, prop.extensionName);
            }) == extensionProps.end())
        {
            printf("Device extension %s requested but not available\n", ext);
            assert(0);
        }
    }
#endif

    vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &count, 0);
    std::vector<VkQueueFamilyProperties> props(count);
    vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &count, props.data());

    u32 family = 0;

    for (auto& prop : props)
    {
        if ((prop.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            (prop.queueFlags & VK_QUEUE_COMPUTE_BIT) &&
            (prop.queueFlags & VK_QUEUE_TRANSFER_BIT))
        {
            break;
        }
        family++;
    }

    float prio = 1.f;

    VkDeviceQueueCreateInfo qinfo = {
        .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = family,
        .queueCount       = 1,
        .pQueuePriorities = &prio,
    };

    VkPhysicalDeviceVulkan12Features vk12features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .timelineSemaphore = VK_TRUE,
    };

    VkPhysicalDeviceVulkan13Features vk13features = {
        .sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext            = &vk12features,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE,
    };

    VkPhysicalDeviceFeatures2 features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &vk13features,
    };

    VkDeviceCreateInfo info = {
        .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext                   = &features,
        .queueCreateInfoCount    = 1,
        .pQueueCreateInfos       = &qinfo,
        .enabledLayerCount       = (u32)layers.size(),
        .ppEnabledLayerNames     = layers.data(),
        .enabledExtensionCount   = (u32)deviceExtensions.size(),
        .ppEnabledExtensionNames = deviceExtensions.data(),
    };

    MZ_VULKAN_ASSERT_SUCCESS(vkCreateDevice(PhysicalDevice, &info, 0, &handle));
    vkl_load_device_functions(handle, this);

    Queue        = Queue::New(this, family, 0);
    ImmAllocator = Allocator::New(this);
    ImmCmdPool   = CommandPool::New(this);
}

Device::~Device()
{
    for (auto& [id, glob] : Globals)
    {
        glob.Free(this);
    }

    ImmAllocator.reset();
    ImmCmdPool.reset();
    DestroyDevice(0);
}

Context::Context()
    : Lib(dynalo::open("vulkan-1.dll"))
{

    MZ_VULKAN_ASSERT_SUCCESS(vkl_init(dynalo::get_function<decltype(vkGetInstanceProcAddr)>((dynalo::native::handle)Lib, "vkGetInstanceProcAddr")));
    u32 count;

    VkApplicationInfo app = {
        .sType      = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .apiVersion = VK_API_VERSION_1_3,
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

#ifndef NDEBUG
    MZ_VULKAN_ASSERT_SUCCESS(vkEnumerateInstanceLayerProperties(&count, 0));
    std::vector<VkLayerProperties> layerProps(count);
    MZ_VULKAN_ASSERT_SUCCESS(vkEnumerateInstanceLayerProperties(&count, layerProps.data()));

    for (auto layer : layers)
    {
        if (std::find_if(layerProps.begin(), layerProps.end(), [=](auto& prop) {
                return 0 == strcmp(layer, prop.layerName);
            }) == layerProps.end())
        {
            printf("Instance layer %s requested but not available\n", layer);
            assert(0);
        }
    }
#endif

    MZ_VULKAN_ASSERT_SUCCESS(vkEnumeratePhysicalDevices(Instance, &count, 0));

    std::vector<VkPhysicalDevice> pdevices(count);
    Devices.reserve(count);

    MZ_VULKAN_ASSERT_SUCCESS(vkEnumeratePhysicalDevices(Instance, &count, pdevices.data()));

    for (auto pdev : pdevices)
    {
        Devices.emplace_back(Device::New(Instance, pdev));
    }
}

Context::~Context()
{
    Devices.clear();

    vkDestroyInstance(Instance, 0);

    dynalo::close((dynalo::native::handle)Lib);
}

rc<Device> Context::CreateDevice(u64 luid) const
{
    for (auto dev : Devices)
    {
        if (dev->GetLuid() == luid)
        {
            return Device::New(Instance, dev->PhysicalDevice);
        }
    }
    return 0;
}

u64 Device::GetLuid() const
{
    VkPhysicalDeviceIDProperties IDProps = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES,
    };

    VkPhysicalDeviceProperties2 props = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &IDProps,
    };

    vkGetPhysicalDeviceProperties2(PhysicalDevice, &props);

    assert(IDProps.deviceLUIDValid);

    return std::bit_cast<u64, u8[VK_LUID_SIZE]>(IDProps.deviceLUID);
}

} // namespace mz::vk