// Copyright MediaZ AS. All Rights Reserved.


// External
#include <vulkan/vulkan_core.h>
#include <dynalo/dynalo.hpp>

// mzVulkan
#include "mzVulkan/Device.h"
#include "mzVulkan/Allocator.h"
#include "mzVulkan/Command.h"

static std::vector<const char*> layers = {
// TODO: Instead of commenting out these as we please, 
//       we can accept a parameter to the engine to enable the validation layer.
//       This can help us in times of need even in production builds.
//
 #ifdef MZ_DEV_BUILD
 #pragma message("Development build: Enabling VK_LAYER_KHRONOS_validation, VK_LAYER_KHRONOS_synchronization2")
     "VK_LAYER_KHRONOS_validation",
      "VK_LAYER_KHRONOS_synchronization2",
 #endif
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
    "VK_KHR_dynamic_rendering",
    "VK_KHR_copy_commands2",
};

namespace mz::vk
{


static std::string GetName(VkPhysicalDevice PhysicalDevice)
{
    VkPhysicalDeviceIDProperties IDProps = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES,
    };

    VkPhysicalDeviceProperties2 props = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &IDProps,
    };

    vkGetPhysicalDeviceProperties2(PhysicalDevice, &props);
    return props.properties.deviceName;
}

bool Device::IsSupported(VkPhysicalDevice PhysicalDevice)
{
    std::string name = vk::GetName(PhysicalDevice);
    bool supported = true;

    u32 count;
    MZ_VULKAN_ASSERT_SUCCESS(vkEnumerateDeviceExtensionProperties(PhysicalDevice, 0, &count, 0));
    std::vector<VkExtensionProperties> extensionProps(count);
    MZ_VULKAN_ASSERT_SUCCESS(vkEnumerateDeviceExtensionProperties(PhysicalDevice, 0, &count, extensionProps.data()));
    
    VkPhysicalDeviceVulkan11Features vk11features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
    };

    VkPhysicalDeviceVulkan12Features vk12features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &vk11features,
    };

    VkPhysicalDeviceVulkan13Features vk13features = {
        .sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext            = &vk12features,
    };

    VkPhysicalDeviceFeatures2 features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &vk13features,
    };

    vkGetPhysicalDeviceFeatures2(PhysicalDevice, &features);

    supported = 
        vk11features.samplerYcbcrConversion &&
        vk12features.timelineSemaphore && 
        vk13features.synchronization2 &&
        vk13features.dynamicRendering;

    if(!vk11features.samplerYcbcrConversion) 
    {
        supported = false;
        printf("%s does not support feature samplerYcbcrConversion\n", name.c_str());
    }
    if(!vk12features.timelineSemaphore) 
    {
        supported = false;
        printf("%s does not support feature timelineSemaphore\n", name.c_str());
    }
    if(!vk13features.synchronization2) 
    {
        supported = false;
        printf("%s does not support feature synchronization2\n", name.c_str());
    }
    if(!vk13features.dynamicRendering) 
    {
        supported = false;
        printf("%s does not support feature dynamicRendering\n", name.c_str());
    }

    for (auto ext : deviceExtensions)
    {
        if (std::find_if(extensionProps.begin(), extensionProps.end(), [=](auto& prop) {
                return 0 == strcmp(ext, prop.extensionName);
            }) == extensionProps.end())
        {
            printf("%s does not support extension: %s\n", name.c_str(), ext);
            supported = false;
        }
    }


    //TODO: add mechanism to fallback into non-dynamic pipeline 
    // when no device suitable for vulkan 1.3 extensions is found 
    //supported = true;

    return supported;
}

bool Device::GetFallbackOptionsForDevice(VkPhysicalDevice PhysicalDevice, mzFallbackOptions& FallbackOptions)
{

    bool supported = true;

    u32 count;
    MZ_VULKAN_ASSERT_SUCCESS(vkEnumerateDeviceExtensionProperties(PhysicalDevice, 0, &count, 0));
    std::vector<VkExtensionProperties> extensionProps(count);
    MZ_VULKAN_ASSERT_SUCCESS(vkEnumerateDeviceExtensionProperties(PhysicalDevice, 0, &count, extensionProps.data()));

    VkPhysicalDeviceVulkan11Features vk11features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
    };

    VkPhysicalDeviceVulkan12Features vk12features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &vk11features,
    };

    VkPhysicalDeviceVulkan13Features vk13features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &vk12features,
    };

    VkPhysicalDeviceFeatures2 features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &vk13features,
    };

    vkGetPhysicalDeviceFeatures2(PhysicalDevice, &features);

    supported =
        vk11features.samplerYcbcrConversion &&
        vk12features.timelineSemaphore;

    u32 instanceVersion = VK_API_VERSION_1_0;
    MZ_VULKAN_ASSERT_SUCCESS(vkEnumerateInstanceVersion(&instanceVersion));
    uint32_t minorVulkanVersion = VK_VERSION_MINOR(instanceVersion);
    
    if (minorVulkanVersion < 3)
    {
        FallbackOptions.mzSync2Fallback = true;
        FallbackOptions.mzDynamicRenderingFallback = true;
        FallbackOptions.mzCopy2Fallback = true;
    }
    else
    {
        if (!vk13features.synchronization2)
        {
            FallbackOptions.mzSync2Fallback = true;
        }
        if (!vk13features.dynamicRendering)
        {
            FallbackOptions.mzDynamicRenderingFallback = true;
        }
    }

    return supported;
}


std::string Device::GetName() const
{
    return vk::GetName(PhysicalDevice);
}

rc<CommandPool> Device::GetPool()
{
    auto& Pool = ImmPools[std::this_thread::get_id()];
    if (!Pool)
    {
        Pool = CommandPool::New(this);
    }
    return Pool;
}

Device::Device(VkInstance Instance, VkPhysicalDevice PhysicalDevice, mzFallbackOptions FallbackOptions)
    : Instance(Instance), PhysicalDevice(PhysicalDevice), FallbackOptions(FallbackOptions)
{
    u32 count;

    MZ_VULKAN_ASSERT_SUCCESS(vkEnumerateDeviceExtensionProperties(PhysicalDevice, 0, &count, 0));
    std::vector<VkExtensionProperties> extensionProps(count);
    MZ_VULKAN_ASSERT_SUCCESS(vkEnumerateDeviceExtensionProperties(PhysicalDevice, 0, &count, extensionProps.data()));

    std::vector<const char*> deviceExtensionsToAsk;

    for (auto ext : deviceExtensions)
    {
        if (std::find_if(extensionProps.begin(), extensionProps.end(), [=](auto& prop) {
                return 0 == strcmp(ext, prop.extensionName);
            }) == extensionProps.end())
        {
            if (strcmp(ext, "VK_KHR_dynamic_rendering") == 0 && FallbackOptions.mzDynamicRenderingFallback)
            {
                printf("Device extension %s requested but not available, fallback mechanism in place\n", ext);
                continue;

            }
            if (strcmp(ext, "VK_KHR_synchronization2") == 0 && FallbackOptions.mzSync2Fallback)
            {
                printf("Device extension %s requested but not available, fallback mechanism in place\n", ext);
                continue;

            }
            if (strcmp(ext, "VK_KHR_copy_commands2") == 0 && FallbackOptions.mzSync2Fallback)
            {
                printf("Device extension %s requested but not available, fallback mechanism in place\n", ext);
                FallbackOptions.mzCopy2Fallback = true;
                continue;
            }
            printf("Device extension %s requested but not available\n", ext);
            assert(0);
        }
        deviceExtensionsToAsk.push_back(ext);
    }

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

    VkPhysicalDeviceVulkan11Features vk11features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .samplerYcbcrConversion = VK_TRUE,
    };

    VkPhysicalDeviceVulkan12Features vk12features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &vk11features,
        .scalarBlockLayout = VK_TRUE,
        .uniformBufferStandardLayout = VK_TRUE,
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
        .features = { 
            .fillModeNonSolid = 1,
            .samplerAnisotropy = 1, 
        },
    };

    VkDeviceCreateInfo info = {
        .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext                   = &features,
        .queueCreateInfoCount    = 1,
        .pQueueCreateInfos       = &qinfo,
        .enabledLayerCount       = (u32)layers.size(),
        .ppEnabledLayerNames     = layers.data(),
        .enabledExtensionCount   = (u32)deviceExtensionsToAsk.size(),
        .ppEnabledExtensionNames = deviceExtensionsToAsk.data(),
    };

    MZ_VULKAN_ASSERT_SUCCESS(vkCreateDevice(PhysicalDevice, &info, 0, &handle));
    vkl_load_device_functions(handle, this);

    Queue        = Queue::New(this, family, 0);
    ImmAllocator = Allocator::New(this);
}

void Context::OrderDevices()
{
    //TODO: Order devices in order to best device to work on is in the first index (Devices[0])

    int idx = 0;
    for (auto device : Devices)
    {
        VkPhysicalDeviceProperties props = {};
        vkGetPhysicalDeviceProperties(device->PhysicalDevice, &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            std::iter_swap(Devices.begin() + idx, Devices.begin());
            break;
        }
        idx++;
    }
}

Device::~Device()
{
    for (auto& [id, glob] : Globals)
    {
        glob.Free(this);
    }

    ImmAllocator.reset();
    // Issue #490.
    // Offending:
    // GetPool().reset();
    // Replaced with below line.
    ImmPools.clear();
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

    MZ_VULKAN_ASSERT_SUCCESS(vkEnumeratePhysicalDevices(Instance, &count, 0));

    std::vector<VkPhysicalDevice> pdevices(count);
    Devices.reserve(count);

    MZ_VULKAN_ASSERT_SUCCESS(vkEnumeratePhysicalDevices(Instance, &count, pdevices.data()));

    for (auto pdev : pdevices)
    {

        
        if(Device::IsSupported(pdev))
        {
            mzFallbackOptions FallbackOptions{
                .mzDynamicRenderingFallback = false,
                .mzSync2Fallback = false,
                .mzCopy2Fallback = false,
            };
            rc<Device> device = Device::New(Instance, pdev, FallbackOptions);
            Devices.emplace_back(device);
            
        }
    }
    if (Devices.empty())
    {
        for (auto pdev : pdevices)
        {
            mzFallbackOptions FallbackOptions{
                .mzDynamicRenderingFallback = false,
                .mzSync2Fallback = false,
                .mzCopy2Fallback = false,
            };
            if (Device::GetFallbackOptionsForDevice(pdev, FallbackOptions))
            {
                rc<Device> device = Device::New(Instance, pdev, FallbackOptions);
                Devices.emplace_back(device);
            }
        }
    }
    if(Devices.empty())
    {
        printf("Currently , we do not support any of your graphics cards \n");
    }
    else
    {
        OrderDevices();
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
            return Device::New(Instance, dev->PhysicalDevice, dev->FallbackOptions);
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
