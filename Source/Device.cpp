// Copyright MediaZ AS. All Rights Reserved.


// External
#include <vulkan/vulkan_core.h>

// nosVulkan
#include "nosVulkan/Common.h"
#include "nosVulkan/Device.h"
#include "nosVulkan/Command.h"
#include "nosVulkan/QueryPool.h"

#include <iostream>
#include <bit>
#include <memory>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

static std::vector<const char*> layers = {
    // "VK_LAYER_KHRONOS_validation",
    // "VK_LAYER_KHRONOS_synchronization2",
};

static std::vector<const char*> extensions = {
    "VK_KHR_surface",
    "VK_KHR_win32_surface",
    "VK_KHR_external_memory_capabilities",
	"VK_KHR_external_semaphore_capabilities",
	"VK_EXT_debug_utils",
	"VK_KHR_get_physical_device_properties2",
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
	"VK_EXT_memory_budget",
    // "VK_NV_external_memory_rdma",
};

namespace nos::vk
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
		{
			std::unique_lock ulock(d->ImmPoolsMutex);
			d->ImmPools.erase(id);
		}
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
    NOSVK_ASSERT(vkEnumerateDeviceExtensionProperties(PhysicalDevice, 0, &count, 0));
    std::vector<VkExtensionProperties> extensionProps(count);
    NOSVK_ASSERT(vkEnumerateDeviceExtensionProperties(PhysicalDevice, 0, &count, extensionProps.data()));
    
#define CHECK_SUPPORT(v, f) if(!v.f) { supported = false; printf("%s does not support feature: "#f"\n", name.c_str());}

    auto set = FeatureSet(PhysicalDevice);
    
    CHECK_SUPPORT(set, samplerYcbcrConversion);
    CHECK_SUPPORT(set, storageBuffer16BitAccess);
    CHECK_SUPPORT(set, uniformAndStorageBuffer16BitAccess);

    CHECK_SUPPORT(set, scalarBlockLayout);
    CHECK_SUPPORT(set, uniformBufferStandardLayout);
    CHECK_SUPPORT(set, hostQueryReset);
    CHECK_SUPPORT(set, timelineSemaphore);

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
    // supported = true;

    return supported;
}

std::string Device::GetName() const
{
    return vk::GetName(PhysicalDevice);
}

void Device::InitializeVMA()
{
    VmaVulkanFunctions funcs {
        .vkGetInstanceProcAddr = &vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = &vkGetDeviceProcAddr
    };

    VkPhysicalDeviceMemoryProperties props;
	vkGetPhysicalDeviceMemoryProperties(PhysicalDevice, &props);
	std::vector<VkExternalMemoryHandleTypeFlagsKHR> handleTypes(props.memoryTypeCount);
	for (int i = 0; i < props.memoryTypeCount; ++i)
	{
		// If the memory type is not BAR/ReBAR memory, we can create memory with external memory handle types
		if (!(props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT &&
			  props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT &&
			  props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
			handleTypes[i] = PLATFORM_EXTERNAL_MEMORY_HANDLE_TYPE;
		else
			handleTypes[i] = 0;
	}

    VmaDeviceMemoryCallbacks deviceMemoryCallbacks = {
		.pfnFree = [](VmaAllocator allocator, uint32_t memoryType, VkDeviceMemory memory, VkDeviceSize size, void* pUserData) {
            auto* Vk = reinterpret_cast<Device*>(pUserData);
            std::lock_guard lock(Vk->MemoryBlocksMutex);
			if (auto it = Vk->MemoryBlocks.find(memory); it != Vk->MemoryBlocks.end())
			{
			    PlatformCloseHandle(it->second);
			    Vk->MemoryBlocks.erase(it);
            }
		}, 
        .pUserData = this
	};

    VmaAllocatorCreateInfo createInfo = {
        .physicalDevice = PhysicalDevice,
        .device = handle,
		.pDeviceMemoryCallbacks = &deviceMemoryCallbacks,
        .pVulkanFunctions = &funcs,
        .instance = Instance,
        .vulkanApiVersion = API_VERSION_USED,
		.pTypeExternalMemoryHandleTypes = handleTypes.data(),
    };
	NOSVK_ASSERT(vmaCreateAllocator(&createInfo, &Allocator));
}

rc<CommandPool> Device::GetPool()
{
	{
	std::shared_lock slock(ImmPoolsMutex);
	auto it = ImmPools.find(std::this_thread::get_id());
	    if (it != ImmPools.end())
	    {
		    return it->second.first;
	    }
    }
	std::unique_lock ulock(ImmPoolsMutex);
	auto& res = ImmPools[std::this_thread::get_id()] = {CommandPool::New(this), QueryPool::New(this)};
    return res.first;
}

rc<QueryPool> Device::GetQPool()
{
	{
		std::shared_lock slock(ImmPoolsMutex);
		auto it = ImmPools.find(std::this_thread::get_id());
		if (it != ImmPools.end())
		{
			return it->second.second;
		}
	}
	std::unique_lock ulock(ImmPoolsMutex);
	auto& res = ImmPools[std::this_thread::get_id()] = {CommandPool::New(this), QueryPool::New(this)};
	return res.second;
}

Device::MemoryUsage Device::GetCurrentMemoryUsage() const
{
	MemoryUsage res{};
	VmaBudget budgets[VK_MAX_MEMORY_HEAPS]{};
	vmaGetHeapBudgets(Allocator, budgets);
	for (uint32_t i = 0; i < MemoryProps.memoryProperties.memoryHeapCount; ++i)
	{
		if (MemoryProps.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
		{
			res.Usage += budgets[i].usage;
			res.Budget += budgets[i].budget;
		}
	}
	return res;
}

Device::Device(VkInstance Instance, VkPhysicalDevice PhysicalDevice)
	: Instance(Instance), PhysicalDevice(PhysicalDevice), Features(PhysicalDevice), ResourcePools({ .Image = std::make_unique<ImagePool>(this), .Buffer = std::make_unique<BufferPool>(this) })
{
	vkGetPhysicalDeviceMemoryProperties2(PhysicalDevice, &MemoryProps);

    u32 count;

    NOSVK_ASSERT(vkEnumerateDeviceExtensionProperties(PhysicalDevice, 0, &count, 0));
    std::vector<VkExtensionProperties> extensionProps(count);
    NOSVK_ASSERT(vkEnumerateDeviceExtensionProperties(PhysicalDevice, 0, &count, extensionProps.data()));

    std::vector<const char*> deviceExtensionsToAsk;

    for (auto ext : deviceExtensions)
    {
        if (std::find_if(extensionProps.begin(), extensionProps.end(), [=](auto& prop) {
                return 0 == strcmp(ext, prop.extensionName);
            }) == extensionProps.end())
        {
            if (strcmp(ext, "VK_KHR_dynamic_rendering") == 0 && !Features.dynamicRendering)
            {
                printf("Device extension %s requested but not available, fallback mechanism in place\n", ext);
                continue;
            }

            if (strcmp(ext, "VK_KHR_synchronization2") == 0 && !Features.synchronization2)
            {
                printf("Device extension %s requested but not available, fallback mechanism in place\n", ext);
                continue;
            }

            if (strcmp(ext, "VK_KHR_copy_commands2") == 0 && !Features.synchronization2)
            {
                printf("Device extension %s requested but not available, fallback mechanism in place\n", ext);
                continue;
            }

            printf("Device extension %s requested but not available\n", ext);
            assert(0);
        }
        else deviceExtensionsToAsk.push_back(ext);
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
    
    set.storageBuffer16BitAccess = VK_TRUE;
    set.uniformAndStorageBuffer16BitAccess = VK_TRUE;
    set.samplerYcbcrConversion = VK_TRUE;
 
    set.scalarBlockLayout = VK_TRUE;
    set.uniformBufferStandardLayout = VK_TRUE;
    set.hostQueryReset = VK_TRUE;
    set.timelineSemaphore = VK_TRUE;

    set.synchronization2 = VK_TRUE;
    set.dynamicRendering = VK_TRUE;

    set.features.fillModeNonSolid = VK_TRUE;
    set.features.samplerAnisotropy = VK_TRUE;

    set.runtimeDescriptorArray = VK_TRUE;

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

    NOSVK_ASSERT(vkCreateDevice(PhysicalDevice, &info, 0, &handle));
    vkl_load_device_functions(handle, this);
    Queue = Queue::New(this, family, 0);
	InitializeVMA();
    GetSampler(VK_FILTER_NEAREST);
    GetSampler(VK_FILTER_LINEAR);
    //GetSampler(VK_FILTER_CUBIC_IMG);
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
	ResourcePools = {};
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
	{
		std::unique_lock ulock(ImmPoolsMutex);
		ImmPools.clear();
	}
	vmaDestroyAllocator(Allocator);
    DestroyDevice(0);
}

static VKAPI_ATTR VkBool32 VKAPI_CALL DefaultDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) 
{

    if(messageSeverity & (VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT))
    {

        std::string msg = pCallbackData->pMessage;
        std::cerr << "validation layer: " << msg << std::endl;
    }

    return VK_FALSE;
}


void Context::EnableValidationLayers(bool enable)
{
    #ifndef NOS_DEV_BUILD
        return;
    #endif
    if(!enable) return layers.clear();
    layers = {
        "VK_LAYER_KHRONOS_validation",
        "VK_LAYER_KHRONOS_synchronization2",
    };
}

Context::Context(DebugCallback* debugCallback)
    : Lib(::LoadLibrary("vulkan-1.dll"))
{
    NOSVK_ASSERT(vkl_init((PFN_vkGetInstanceProcAddr)GetProcAddress((HMODULE)Lib, "vkGetInstanceProcAddr")));
    u32 count;

    VkApplicationInfo app = {
        .sType      = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .apiVersion = API_VERSION_USED
    };

    VkInstanceCreateInfo info = {
        .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo        = &app,
        .enabledLayerCount       = (u32)layers.size(),
        .ppEnabledLayerNames     = layers.data(),
        .enabledExtensionCount   = (u32)extensions.size(),
        .ppEnabledExtensionNames = extensions.data(),
    };

    NOSVK_ASSERT(vkCreateInstance(&info, 0, &Instance));
    vkl_load_instance_functions(Instance);

    if(!debugCallback)
		debugCallback = DefaultDebugCallback;

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
	NOSVK_ASSERT(vkCreateDebugUtilsMessengerEXT(Instance, &msgInfo, 0, &Msger));

    NOSVK_ASSERT(vkEnumerateInstanceLayerProperties(&count, 0));
    std::vector<VkLayerProperties> layerProps(count);
    NOSVK_ASSERT(vkEnumerateInstanceLayerProperties(&count, layerProps.data()));

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

    NOSVK_ASSERT(vkEnumeratePhysicalDevices(Instance, &count, 0));

    std::vector<VkPhysicalDevice> pdevices(count);
    Devices.reserve(count);

    NOSVK_ASSERT(vkEnumeratePhysicalDevices(Instance, &count, pdevices.data()));

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

    ::FreeLibrary((HMODULE)Lib);
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
        NOSVK_ASSERT(CreateSampler(&info, 0, &sampler));
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

} // namespace nos::vk
