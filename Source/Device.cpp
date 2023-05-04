// Copyright MediaZ AS. All Rights Reserved.


// External
#include <vulkan/vulkan_core.h>
#include <dynalo/dynalo.hpp>

// mzVulkan
#include "mzVulkan/Common.h"
#include "mzVulkan/Device.h"
#include "mzVulkan/Allocator.h"
#include "mzVulkan/Command.h"
#include "mzVulkan/QueryPool.h"

static std::vector<const char*> layers = {
};

static std::vector<const char*> extensions = {
    "VK_KHR_surface",
    "VK_KHR_win32_surface",
    "VK_KHR_external_memory_capabilities",
    "VK_EXT_debug_utils",
};

static std::vector<const char*> deviceExtensions = {
    "VK_KHR_swapchain",
    "VK_KHR_external_semaphore_win32",
    "VK_KHR_external_memory_win32",
    "VK_EXT_external_memory_host",
    "VK_KHR_synchronization2",
    "VK_KHR_dynamic_rendering",
    "VK_KHR_copy_commands2",
    "VK_EXT_host_query_reset",
    "VK_KHR_shader_float16_int8",
    "VK_KHR_16bit_storage",
    // "VK_NV_external_memory_rdma",
};

namespace mz::vk
{

static std::mutex Lock;
static std::set<Device*> Devices;

thread_local struct PoolCleaner
{
    ~PoolCleaner()
    {
        std::lock_guard lock(Lock);
        auto id = std::this_thread::get_id();
        for (auto d : Devices)
            d->ImmPools.erase(id);
    }
} PoolCleaner;

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


bool Device::CheckSupport(VkPhysicalDevice PhysicalDevice)
{
    std::string name = vk::GetName(PhysicalDevice);
    bool supported = true;

    u32 count;
    MZVK_ASSERT(vkEnumerateDeviceExtensionProperties(PhysicalDevice, 0, &count, 0));
    std::vector<VkExtensionProperties> extensionProps(count);
    MZVK_ASSERT(vkEnumerateDeviceExtensionProperties(PhysicalDevice, 0, &count, extensionProps.data()));
    
#define CHECK_SUPPORT(v, f) if(!v.f) { supported = false; printf("%s does not support feature: "#f"\n", name.c_str());}

    auto set = FeatureSet(PhysicalDevice);
    
    CHECK_SUPPORT(set.vk11, samplerYcbcrConversion);
    CHECK_SUPPORT(set.vk11, storageBuffer16BitAccess);
    CHECK_SUPPORT(set.vk11, uniformAndStorageBuffer16BitAccess);

    CHECK_SUPPORT(set.vk12, scalarBlockLayout);
    CHECK_SUPPORT(set.vk12, uniformBufferStandardLayout);
    CHECK_SUPPORT(set.vk12, hostQueryReset);
    CHECK_SUPPORT(set.vk12, timelineSemaphore);

    // These have fallbacks
    // CHECK_SUPPORT(set.vk13, synchronization2);
    // CHECK_SUPPORT(set.vk13, dynamicRendering);

    CHECK_SUPPORT(set.features, fillModeNonSolid);
    CHECK_SUPPORT(set.features, samplerAnisotropy);
    
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
    supported = true;

    return supported;
}

std::string Device::GetName() const
{
    return vk::GetName(PhysicalDevice);
}

rc<CommandPool> Device::GetPool()
{
    auto& Pool = ImmPools[std::this_thread::get_id()];
    if (!Pool.first)
    {
        Pool = {CommandPool::New(this), QueryPool::New(this)};
    }
    return Pool.first;
}

rc<QueryPool> Device::GetQPool()
{
    auto& Pool = ImmPools[std::this_thread::get_id()];
    if (!Pool.first)
    {
        Pool = { CommandPool::New(this), QueryPool::New(this) };
    }
    return Pool.second;
}

Device::Device(VkInstance Instance, VkPhysicalDevice PhysicalDevice)
    : Instance(Instance), PhysicalDevice(PhysicalDevice), Features(PhysicalDevice)
{
    u32 count;

    MZVK_ASSERT(vkEnumerateDeviceExtensionProperties(PhysicalDevice, 0, &count, 0));
    std::vector<VkExtensionProperties> extensionProps(count);
    MZVK_ASSERT(vkEnumerateDeviceExtensionProperties(PhysicalDevice, 0, &count, extensionProps.data()));

    std::vector<const char*> deviceExtensionsToAsk;

    for (auto ext : deviceExtensions)
    {
        if (std::find_if(extensionProps.begin(), extensionProps.end(), [=](auto& prop) {
                return 0 == strcmp(ext, prop.extensionName);
            }) == extensionProps.end())
        {
            if (strcmp(ext, "VK_KHR_dynamic_rendering") == 0 && !Features.vk13.dynamicRendering)
            {
                printf("Device extension %s requested but not available, fallback mechanism in place\n", ext);
                continue;
            }

            if (strcmp(ext, "VK_KHR_synchronization2") == 0 && !Features.vk13.synchronization2)
            {
                printf("Device extension %s requested but not available, fallback mechanism in place\n", ext);
                continue;
            }

            if (strcmp(ext, "VK_KHR_copy_commands2") == 0 && !Features.vk13.synchronization2)
            {
                printf("Device extension %s requested but not available, fallback mechanism in place\n", ext);
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

    FeatureSet set;
    set.vk11 = {
        .storageBuffer16BitAccess = VK_TRUE,
        .uniformAndStorageBuffer16BitAccess = VK_TRUE,
        .samplerYcbcrConversion = VK_TRUE,
    };
    set.vk12 = {
        .scalarBlockLayout = VK_TRUE,
        .uniformBufferStandardLayout = VK_TRUE,
        .hostQueryReset = VK_TRUE,
        .timelineSemaphore = VK_TRUE,
    };
    set.vk13 = {
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE,
    };
    set.features = {
        .fillModeNonSolid = VK_TRUE,
        .samplerAnisotropy = VK_TRUE,
    };

    auto available = Features & set;
    VkDeviceCreateInfo info = {
        .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext                   = available.pnext(),
        .queueCreateInfoCount    = 1,
        .pQueueCreateInfos       = &qinfo,
        .enabledLayerCount       = (u32)layers.size(),
        .ppEnabledLayerNames     = layers.data(),
        .enabledExtensionCount   = (u32)deviceExtensionsToAsk.size(),
        .ppEnabledExtensionNames = deviceExtensionsToAsk.data(),
    };

    MZVK_ASSERT(vkCreateDevice(PhysicalDevice, &info, 0, &handle));
    vkl_load_device_functions(handle, this);
    Queue        = Queue::New(this, family, 0);
    ImmAllocator = Allocator::New(this);
    GetSampler(VK_FILTER_NEAREST);
    GetSampler(VK_FILTER_LINEAR);
    GetSampler(VK_FILTER_CUBIC_IMG);
    std::lock_guard lock(Lock);
    Devices.insert(this);
}

void Context::OrderDevices()
{
    //TODO: Order devices in order to best device to work on is in the first index (Devices[0])
    std::sort(Devices.begin(), Devices.end(), [](auto a, auto b) {
        VkPhysicalDeviceProperties props[2] = {};
        vkGetPhysicalDeviceProperties(a->PhysicalDevice, &props[0]);
        vkGetPhysicalDeviceProperties(b->PhysicalDevice, &props[1]);
        return
            (VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU == props[0].deviceType) > 
            (VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU == props[1].deviceType);
    });
}

Device::~Device()
{
    {
        std::lock_guard lock(Lock);
        Devices.erase(this);
    }

    for (auto& [id, glob] : Globals)
    {
        glob.Free(this);
    }

    for(auto& [_, sampler] : Samplers)
    {
        DestroySampler(sampler, 0);
    }

    DeviceWaitIdle();
    ImmPools.clear();
    ImmAllocator.reset();
    DestroyDevice(0);
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}


void Context::EnableValidationLayers(bool enable)
{
    #ifndef MZ_DEV_BUILD
        return;
    #endif
    if(!enable) return layers.clear();
    layers = {
        "VK_LAYER_KHRONOS_validation",
        "VK_LAYER_KHRONOS_synchronization2",
    };
}

Context::Context(DebugCallback* debugCallback)
    : Lib(dynalo::open("vulkan-1.dll"))
{
    MZVK_ASSERT(vkl_init(dynalo::get_function<decltype(vkGetInstanceProcAddr)>((dynalo::native::handle)Lib, "vkGetInstanceProcAddr")));
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

    MZVK_ASSERT(vkCreateInstance(&info, 0, &Instance));
    vkl_load_instance_functions(Instance);

    if(debugCallback)
    {
        VkDebugUtilsMessengerCreateInfoEXT msgInfo = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,

            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
                            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = 
                            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                            | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                            | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = debugCallback,
        };
        MZVK_ASSERT(vkCreateDebugUtilsMessengerEXT(Instance, &msgInfo, 0, &Msger));
    }

    MZVK_ASSERT(vkEnumerateInstanceLayerProperties(&count, 0));
    std::vector<VkLayerProperties> layerProps(count);
    MZVK_ASSERT(vkEnumerateInstanceLayerProperties(&count, layerProps.data()));

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

    MZVK_ASSERT(vkEnumeratePhysicalDevices(Instance, &count, 0));

    std::vector<VkPhysicalDevice> pdevices(count);
    Devices.reserve(count);

    MZVK_ASSERT(vkEnumeratePhysicalDevices(Instance, &count, pdevices.data()));

    for (auto pdev : pdevices)
    {
        if(Device::CheckSupport(pdev))
        {
            rc<Device> device = Device::New(Instance, pdev);
            Devices.emplace_back(device);
        }
    }

    if(Devices.empty())
    {
        printf("We do not support any of your graphics cards currently\n");
        return;
    }
    OrderDevices();
}

Context::~Context()
{
    Devices.clear();

    if(Msger)
    {
        vkDestroyDebugUtilsMessengerEXT(Instance, Msger, 0);
    }
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

VkSampler Device::GetSampler(VkSamplerCreateInfo const& info)
{
    auto& sampler = Samplers[info];
    if(!sampler)
        MZVK_ASSERT(CreateSampler(&info, 0, &sampler));
    return sampler;
}

VkSampler Device::GetSampler(VkFilter Filter)
{
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(PhysicalDevice, &props);
    VkSamplerCreateInfo info = {
        .sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter        = Filter,
        .minFilter        = Filter,
        .mipmapMode       = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias       = 0.0f,
        .anisotropyEnable = 1,
        .maxAnisotropy    = props.limits.maxSamplerAnisotropy,
        .compareOp        = VK_COMPARE_OP_NEVER,
        .maxLod           = 1.f,
        .borderColor      = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
    };
    return GetSampler(info);
}

} // namespace mz::vk
