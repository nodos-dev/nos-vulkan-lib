// Copyright MediaZ Teknoloji A.S. All Rights Reserved.


// External
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan.hpp>

// nosVulkan
#include "nosVulkan/Common.h"
#include "nosVulkan/Device.h"
#include "nosVulkan/Command.h"
#include "nosVulkan/QueryPool.h"
#include "nosVulkan/Platform.h"

#include <iostream>
#include <bit>
#include <memory>
#include <fstream>

#define ENABLE_RENDERDOC_SUPPORT 0

static std::vector<const char*> layers = {
	// "VK_LAYER_KHRONOS_validation",
	// "VK_LAYER_KHRONOS_synchronization2",
};

static std::vector<const char*> extensions = {
	"VK_KHR_surface",
#if defined (_WIN32)
	"VK_KHR_win32_surface",
#elif defined (__linux__)
	"VK_KHR_xcb_surface",
#endif
	"VK_KHR_external_memory_capabilities",
	"VK_KHR_external_semaphore_capabilities",
	"VK_EXT_debug_utils",
	"VK_KHR_get_physical_device_properties2",
};

static std::vector<const char*> deviceExtensions = {
	"VK_KHR_swapchain",
	"VK_KHR_external_semaphore",
#if defined (_WIN32)
	"VK_KHR_external_semaphore_win32",
	"VK_KHR_external_memory_win32",
#elif defined (__linux__)
	"VK_KHR_external_semaphore_fd",
	"VK_KHR_external_memory_fd",
#endif
#if !ENABLE_RENDERDOC_SUPPORT
	"VK_EXT_external_memory_host",
#endif
	"VK_KHR_synchronization2",
	"VK_KHR_dynamic_rendering",
	"VK_KHR_copy_commands2",
	"VK_EXT_host_query_reset",
	"VK_KHR_shader_float16_int8",
	"VK_KHR_16bit_storage",
	"VK_EXT_memory_budget",
	// "VK_NV_external_memory_rdma",
};
static constexpr char PIPELINE_CACHE_FILE_PREFIX[] = "PipelineCache_";

static constexpr uint32_t NOS_VULKAN_INIT_TRY_TIMEOUT_IN_MILLISECONDS = 10 * 1000; // 10 SECONDS
static constexpr uint32_t NOS_VULKAN_INIT_TRY_WAIT_TIMEOUT_IN_MILLISECONDS = 200;
#include <vulkan/vulkan_to_string.hpp>
// Define the macro
#define NOS_VULKAN_KEEP_TRYING(func, log) \
{\
	int32_t timeout = NOS_VULKAN_INIT_TRY_TIMEOUT_IN_MILLISECONDS; \
	VkResult vulkanFuncResult = (func); \
	while (NOS_VULKAN_FAILED(vulkanFuncResult)) { \
		std::this_thread::sleep_for(std::chrono::milliseconds(NOS_VULKAN_INIT_TRY_WAIT_TIMEOUT_IN_MILLISECONDS)); \
		timeout -= NOS_VULKAN_INIT_TRY_WAIT_TIMEOUT_IN_MILLISECONDS; \
		if (timeout <= 0) { \
			printf("%s, VkResult: %s", log, std::to_string(vulkanFuncResult).c_str()); \
			return; \
		} \
	} \
}

// Define the macro
#define NOS_VULKAN_KEEP_TRYING_INVALIDATE(func, log, handleToInvalidate) \
{\
	int32_t timeout = NOS_VULKAN_INIT_TRY_TIMEOUT_IN_MILLISECONDS; \
	VkResult vulkanFuncResult = (func); \
	while (NOS_VULKAN_FAILED(vulkanFuncResult)) { \
		std::this_thread::sleep_for(std::chrono::milliseconds(NOS_VULKAN_INIT_TRY_WAIT_TIMEOUT_IN_MILLISECONDS)); \
		timeout -= NOS_VULKAN_INIT_TRY_WAIT_TIMEOUT_IN_MILLISECONDS; \
		if (timeout <= 0) { \
			printf("%s, VkResult: %s", log, std::to_string(vulkanFuncResult).c_str()); \
			if (handleToInvalidate)\
				handleToInvalidate = NOS_VULKAN_INVALID_HANDLE(decltype(handleToInvalidate));\
			return; \
		} \
	} \
}

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

	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties(PhysicalDevice, &props);

	if(props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)
	{
		GLog.W("Device %s is CPU. No GPU support", name.c_str());
		return false;
	}

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
	// CHECK_SUPPORT(set.vk13, dynamicRendering); // Fallback is not working properly

	CHECK_SUPPORT(set.features, fillModeNonSolid);
	CHECK_SUPPORT(set.features, samplerAnisotropy);
	
	for (auto ext : deviceExtensions)
	{
		if (std::find_if(extensionProps.begin(), extensionProps.end(), [=](auto& prop) {
				return 0 == strcmp(ext, prop.extensionName);
			}) == extensionProps.end())
		{
			printf("%s does not support extension: %s\n", name.c_str(), ext);
			if (ext == "VK_KHR_dynamic_rendering")
				GLog.E("Device %s does not support dynamic rendering. Therefore you can't use graphics pipeline related operations.\n", name.c_str());
			else
				supported = false;
		}
	}

	//TODO: add mechanism to fallback into non-dynamic pipeline 
	// when no device suitable for vulkan 1.3 extensions is found 
	//supported = true;

	return supported;
}

std::string Device::GetName() const
{
	return vk::GetName(PhysicalDevice);
}

std::string Device::GetVendorName() const
{
	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties(PhysicalDevice, &props);
	switch (props.vendorID)
	{
		case 0x10DE: return "NVIDIA";
		case 0x1002: return "AMD";
		case 0x8086: return "Intel";
		case 0x13B5: return "Qualcomm";
		case 0x1A03: return "ARM";
		case 0x102B: return "Matrox Electronic Systems Ltd.";
		default: {
			// Hexadecimal representation of vendor ID
			std::stringstream ss;
			ss << "Vendor 0x" << std::hex << props.vendorID;
			return ss.str();
		}
	}
}

std::string Device::GetModelName() const
{
	auto vendorName = GetVendorName();
	auto deviceName = GetName();
	// If vendor name does not start with Vendor 0x, we can remove it from deviceName
	if (vendorName.find("Vendor 0x") != std::string::npos)
	{
		return deviceName;
	}
	else
	{
		// Remove vendor name from device name
		auto vendorPrefix = vendorName + " ";
		if (deviceName.find(vendorPrefix) == 0)
			return deviceName.substr(vendorPrefix.length());
		else
			return deviceName;
	}
}

std::optional<std::filesystem::path> GetPipelineCacheFilePath(Device* Vk)
{
	if (!Vk->Context->CacheFolder)
		return std::nullopt;
	std::error_code ec;
	if (!std::filesystem::exists(*Vk->Context->CacheFolder, ec))
	{
		if (!ec)
			std::filesystem::create_directories(*Vk->Context->CacheFolder, ec);
	}
	if (ec)
	{
		GLog.E("Failed to create cache folder at %s. Pipeline cache will not be used. Error: %s", Vk->Context->CacheFolder->string().c_str(), ec.message().c_str());
		return std::nullopt;
	}
	std::string fileName = PIPELINE_CACHE_FILE_PREFIX + Vk->GetName() + ".bin";
	SanitizeCacheFileName(fileName);
	return *Vk->Context->CacheFolder / fileName;
}

void CreateDevicePipelineCache(Device* Vk)
{
	auto cacheFile = GetPipelineCacheFilePath(Vk);
	if (!cacheFile)
	{
		GLog.D("Pipeline cache file path is not available for device %s. Pipeline cache will not be used.", Vk->GetName().c_str());
		return;
	}
	std::ifstream file(*cacheFile, std::ios::binary | std::ios::ate);
	std::vector<char> buffer;
	if (file.is_open())
	{
		std::streamsize size = file.tellg();
		file.seekg(0, std::ios::beg);

		buffer.resize(size);
		file.read(buffer.data(), size);
	}


	VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
	pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	pipelineCacheCreateInfo.initialDataSize = buffer.size();
	pipelineCacheCreateInfo.pInitialData = buffer.size() ? buffer.data() : nullptr;
	auto res = vkCreatePipelineCache(Vk->handle, &pipelineCacheCreateInfo, nullptr, &Vk->PipelineCache);
	if (VK_SUCCESS != res)
		GLog.E("Failed to create pipeline cache for device %s", Vk->GetName().c_str());
}

void DestroyDevicePipelineCache(Device* Vk)
{
	if (Vk->PipelineCache == VK_NULL_HANDLE)
		return;
	size_t size = 0;
	vkGetPipelineCacheData(Vk->handle, Vk->PipelineCache, &size, nullptr);
	std::vector<char> buffer(size);
	vkGetPipelineCacheData(Vk->handle, Vk->PipelineCache, &size, buffer.data());
	
	if (auto cacheFile = GetPipelineCacheFilePath(Vk))
	{
		std::ofstream file(*cacheFile, std::ios::binary);
		if (file.is_open())
		{
			file.write(buffer.data(), size);
		}
	}

	vkDestroyPipelineCache(Vk->handle, Vk->PipelineCache, nullptr);
}

void Device::PreAllocateTempMemoryPools()
{
	// Create pre-allocated memory pool for temporary buffers/images
	std::unordered_set<uint32_t> preAllocatedTempMemTypeIndices;
	static constexpr auto addIfValid = [](std::unordered_set<uint32_t>& indices, uint32_t idx){
		if (idx != UINT32_MAX)
			indices.insert(idx);
	};
	if (auto bufferCreateInfo = CalculateBufferCreationInfos(this, GetBufferCreateRequestForTempUploadBuffer(0)).Get())
		addIfValid(preAllocatedTempMemTypeIndices, bufferCreateInfo->MemoryTypeIndex);
	if (auto imageCreateInfo = CalculateImageCreationInfos(this, GetTempImageCreateRequest({ 0, 0 }, VK_FORMAT_R8G8B8A8_UNORM)).Get())
		addIfValid(preAllocatedTempMemTypeIndices, imageCreateInfo->MemoryTypeIndex);
	for (auto& i : preAllocatedTempMemTypeIndices)
	{
		VmaPoolCreateInfo memPoolCreateInfo {
			.memoryTypeIndex = i,
			.flags = 0,
			.blockSize = TEMP_MEMORY_POOL_BLOCK_SIZE,
			.minBlockCount = 2,
			.maxBlockCount = UINT64_MAX,
		};
		VmaPool pool;
		auto res = vmaCreatePool(Allocator, &memPoolCreateInfo, &pool);
		if (VK_SUCCESS == res)
		{
			GLog.I("Created memory pool for memory type index %u", i);
			TempMemoryPools[i] = pool;
		}
		else
			GLog.W("Unable to create memory pool for memory type index %u. Creating temporary buffers with this memory type can impact performance.", i);
	}
}

void Device::OnMemoryFreed(VkDeviceMemory memory)
{
	std::lock_guard lock(MemoryBlocksMutex);
	if (auto it = MemoryBlocks.find(memory); it != MemoryBlocks.end())
	{
		GHandleImporter.CloseHandle(NOS_HANDLE(it->second));
		MemoryBlocks.erase(it);
	}
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
	for (uint32_t i = 0; i < props.memoryTypeCount; ++i)
	{
		// If the memory type is not BAR/ReBAR memory, we can create memory with external memory handle types
		auto& memType = props.memoryTypes[i];
		if (!(memType.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT &&
			  memType.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT &&
			  memType.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
			handleTypes[i] = PLATFORM_EXTERNAL_MEMORY_HANDLE_TYPE;
		else
			handleTypes[i] = 0;
	}

	VmaDeviceMemoryCallbacks deviceMemoryCallbacks = {
		.pfnFree = [](VmaAllocator allocator, uint32_t memoryType, VkDeviceMemory memory, VkDeviceSize size, void* pUserData) {
			auto* Vk = reinterpret_cast<Device*>(pUserData);
			Vk->OnMemoryFreed(memory);
		},
		.pUserData = this
	};

	VmaAllocatorCreateInfo createInfo = {
		.physicalDevice = PhysicalDevice,
		.device = handle,
		.pDeviceMemoryCallbacks = &deviceMemoryCallbacks,
		.pVulkanFunctions = &funcs,
		.instance = Instance,
		.vulkanApiVersion = Context->ApiVersion,
		.pTypeExternalMemoryHandleTypes = handleTypes.data(),
	};
	NOSVK_ASSERT(vmaCreateAllocator(&createInfo, &Allocator));

	PreAllocateTempMemoryPools();
}

rc<CommandPool> Device::GetCommandPool()
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

rc<QueryPool> Device::GetQueryPool()
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

static std::optional<uint32_t> FindDedicatedQueue(VkQueueFlagBits queue, std::vector<VkQueueFamilyProperties> const& props)
{
	std::optional<uint32_t> foundQueue = std::nullopt;
	uint32_t minExtraFlags = UINT32_MAX;

	for (uint32_t i = 0; i < props.size(); ++i)
	{
		VkQueueFlags flags = props[i].queueFlags;

		if (flags & queue)
		{
			// Count how many other bits are set besides desired queue flag
			VkQueueFlags extraFlags = flags & ~queue;

			// Count bits set in extraFlags (number of non-transfer bits)
			uint32_t extraBitCount = 0;
			for (uint32_t b = extraFlags; b != 0; b >>= 1)
			{
				extraBitCount += (b & 1);
			}

			// Prefer fewer extra flags
			if (extraBitCount < minExtraFlags)
			{
				foundQueue = i;
				minExtraFlags = extraBitCount;

				// If no extra flags, this is the ideal queue
				if (extraBitCount == 0)
					break;
			}
		}
	}
	return foundQueue;
}

Device::Device(VkInstance Instance, VkPhysicalDevice PhysicalDevice, const nos::vk::Context* context)
	: Instance(Instance), PhysicalDevice(PhysicalDevice), Features(PhysicalDevice), ResourcePools(this), Context(context)
{
	vkGetPhysicalDeviceMemoryProperties2(PhysicalDevice, &MemoryProps);

	u32 count;

	NOS_VULKAN_KEEP_TRYING_INVALIDATE(vkEnumerateDeviceExtensionProperties(PhysicalDevice, 0, &count, 0), "Failed to find physical device extensions", PhysicalDevice);
	std::vector<VkExtensionProperties> extensionProps(count);
	NOS_VULKAN_KEEP_TRYING_INVALIDATE(vkEnumerateDeviceExtensionProperties(PhysicalDevice, 0, &count, extensionProps.data()), "Failed to access physical device extensions", PhysicalDevice);

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
			return;
		}
		else deviceExtensionsToAsk.push_back(ext);
	}
	
	vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &count, 0);
	std::vector<VkQueueFamilyProperties> props(count);
	vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &count, props.data());

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	float priority = 1.f;

	auto mainQueueFamilyIdx = FindDedicatedQueue(VkQueueFlagBits(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT), props);
	if (mainQueueFamilyIdx)
	{
		VkDeviceQueueCreateInfo graphicsQueueCreateInfo = {
			.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = *mainQueueFamilyIdx,
			.queueCount       = 1,
			.pQueuePriorities = &priority,
		};
		queueCreateInfos.push_back(graphicsQueueCreateInfo);
	}

	auto transferQueueFamilyIdx = FindDedicatedQueue(VK_QUEUE_TRANSFER_BIT, props);
	if (transferQueueFamilyIdx)
	{
		VkDeviceQueueCreateInfo transferQueueCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = *transferQueueFamilyIdx,
			.queueCount = 1,
			.pQueuePriorities = &priority,
		};
		queueCreateInfos.push_back(transferQueueCreateInfo);
	}

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
		.queueCreateInfoCount    = (u32)queueCreateInfos.size(),
		.pQueueCreateInfos       = queueCreateInfos.data(),
		.enabledLayerCount       = (u32)layers.size(),
		.ppEnabledLayerNames     = layers.data(),
		.enabledExtensionCount   = (u32)deviceExtensionsToAsk.size(),
		.ppEnabledExtensionNames = deviceExtensionsToAsk.data(),
	};

	NOS_VULKAN_KEEP_TRYING_INVALIDATE(vkCreateDevice(PhysicalDevice, &info, 0, &handle), "Failed to create logical Vulkan device\n", handle);
	vkl_load_device_functions(handle, this);
	if (mainQueueFamilyIdx)
		MainQueue = Queue::New(this, *mainQueueFamilyIdx);
	if (transferQueueFamilyIdx)
		TransferQueue = Queue::New(this, *transferQueueFamilyIdx);
	InitializeVMA();
	GetSampler(VK_FILTER_NEAREST);
	GetSampler(VK_FILTER_LINEAR);
	//GetSampler(VK_FILTER_CUBIC_IMG);
	CreateDevicePipelineCache(this);
	std::lock_guard lock(Lock);
	Devices.insert(this);
}

void Context::OrderAndFilterDevices(std::vector<VkPhysicalDevice>& PhysicalDevices)
{
	for (auto it = PhysicalDevices.begin(); it != PhysicalDevices.end();)
	{
		GLog.I("Checking device %s for support", GetName(*it).c_str());
		if (Device::CheckSupport(*it))
			++it;
		else
		{
			GLog.W("Device %s does not support required features, skipping", GetName(*it).c_str());
			it = PhysicalDevices.erase(it);
		}
	}
	//TODO: Order devices in order to best device to work on is in the first index (Devices[0])
	std::sort(PhysicalDevices.begin(), PhysicalDevices.end(), [](auto a, auto b) {
		VkPhysicalDeviceProperties props[2] = {};
		vkGetPhysicalDeviceProperties(a, &props[0]);
		vkGetPhysicalDeviceProperties(b, &props[1]);
		// If both are discrete, prefer same major-bigger minor version
		if ((VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU == props[0].deviceType) ==
			(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU == props[1].deviceType))
		{
			if (VK_API_VERSION_MAJOR(props[0].apiVersion) != VK_API_VERSION_MAJOR(MAX_API_VERSION_USED) ||
				VK_API_VERSION_MAJOR(props[1].apiVersion) != VK_API_VERSION_MAJOR(MAX_API_VERSION_USED))
				return (VK_API_VERSION_MAJOR(props[0].apiVersion) == VK_API_VERSION_MAJOR(MAX_API_VERSION_USED)) >
					   (VK_API_VERSION_MAJOR(props[1].apiVersion) == VK_API_VERSION_MAJOR(MAX_API_VERSION_USED));
			// Prefer the GPU with bigger minor version
			if (VK_API_VERSION_MINOR(props[0].apiVersion) != VK_API_VERSION_MINOR(props[1].apiVersion))
				return VK_API_VERSION_MINOR(props[0].apiVersion) > VK_API_VERSION_MINOR(props[1].apiVersion);
			return false;
		}
		return
			(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU == props[0].deviceType) > 
			(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU == props[1].deviceType);
	});
}

Device::~Device()
{
	if (handle == NOS_VULKAN_INVALID_HANDLE(VkDevice))
		return;

	DestroyDevicePipelineCache(this);

	ResourcePools.Clear();
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
	for (auto& [memTypeIndex, pool] : TempMemoryPools)
	{
		vmaDestroyPool(Allocator, pool);
	}
	MainQueue = nullptr;
	TransferQueue = nullptr;
	Globals.clear();
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
	if(!enable) return layers.clear();
	layers = {
		"VK_LAYER_KHRONOS_validation",
		"VK_LAYER_KHRONOS_synchronization2",
	};
}
Context::Context(DebugCallback* debugCallback, std::optional<std::filesystem::path> cacheFolder, bool enableValidationLayer)
	: CacheFolder(std::move(cacheFolder))
{
	std::vector<VkPhysicalDevice> pDevices;
	auto createInstance = [&]() {
		try
		{
			vkLoader = std::make_unique<::vk::DynamicLoader>();
			NOSVK_ASSERT(vkl_init(vkLoader->getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr")));
		}
		catch (std::exception& e)
		{
			printf("Failed to load Vulkan library: %s\n", e.what());
			assert(0);
		}
		VkApplicationInfo app = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = ApiVersion};
		EnableValidationLayers(enableValidationLayer);

		VkInstanceCreateInfo info = {
		.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo        = &app,
		.enabledLayerCount       = (u32)layers.size(),
		.ppEnabledLayerNames     = layers.data(),
		.enabledExtensionCount   = (u32)extensions.size(),
			.ppEnabledExtensionNames = extensions.data(),
		};

	NOS_VULKAN_KEEP_TRYING_INVALIDATE(vkCreateInstance(&info, 0, &Instance), "Failed to create Vulkan instance!\n", Instance);

		vkl_load_instance_functions(Instance);

		uint32_t count = 0;
		NOS_VULKAN_KEEP_TRYING(vkEnumeratePhysicalDevices(Instance, &count, 0),
							   "Failed to find physical Vulkan device that supports the Vulkan version\n");

		pDevices.resize(count);
		NOS_VULKAN_KEEP_TRYING(vkEnumeratePhysicalDevices(Instance, &count, pDevices.data()),
							   "Failed to access found physical Vulkan devices\n");
		Devices.reserve(count);
	};

	createInstance();
	if (Instance == NOS_VULKAN_INVALID_HANDLE(VkInstance))
		return;


	// Detect the proper Vulkan instance version with most capable Vulkan device
	OrderAndFilterDevices(pDevices);

	if (pDevices.size() == 0)
	{
		GLog.E("No suitable Vulkan devices found\n");
		return;
	}

	{
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(pDevices[0], &props);

		uint32_t firstDeviceApiVersion = VK_MAKE_API_VERSION(0, VK_API_VERSION_MAJOR(props.apiVersion), VK_API_VERSION_MINOR(props.apiVersion), 0);
		if (firstDeviceApiVersion >= MAX_API_VERSION_USED)
			ApiVersion = MAX_API_VERSION_USED;
		else
		{
			GLog.E("Most capable Vulkan device doesn't support Vulkan version used by backend. There may be unexpected crashes!");
			ApiVersion = firstDeviceApiVersion;
		}

		vkDestroyInstance(Instance, 0);
	}
	createInstance();
	if (Instance == NOS_VULKAN_INVALID_HANDLE(VkInstance))
		return;

	OrderAndFilterDevices(pDevices);


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
	NOS_VULKAN_KEEP_TRYING_INVALIDATE(vkCreateDebugUtilsMessengerEXT(Instance, &msgInfo, 0, &Msger), "Failed to find Vulkan Debug Utils\n", Msger);

	u32 layerCount;
	NOSVK_ASSERT(vkEnumerateInstanceLayerProperties(&layerCount, 0));
	std::vector<VkLayerProperties> layerProps(layerCount);
	NOSVK_ASSERT(vkEnumerateInstanceLayerProperties(&layerCount, layerProps.data()));

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

	for (auto pdev : pDevices)
	{
		if(Device::CheckSupport(pdev))
		{
			rc<Device> device = Device::New(Instance, pdev, this);
			Devices.emplace_back(device);
#if SUITABLE_FOR_RENDERDOC
			break;
#endif
		}
	}

	if(Devices.empty())
	{
		printf("We do not support any of your graphics cards currently\n");
		return;
	}
}

Context::~Context()
{
	Devices.clear();

	if(Msger && Msger != NOS_VULKAN_INVALID_HANDLE(VkDebugUtilsMessengerEXT))
		vkDestroyDebugUtilsMessengerEXT(Instance, Msger, 0);
	
	if(Instance && Instance != NOS_VULKAN_INVALID_HANDLE(VkInstance))
		vkDestroyInstance(Instance, 0);
}

Device::UID Device::GetLuid() const
{
	VkPhysicalDeviceIDProperties IDProps = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES,
	};

	VkPhysicalDeviceProperties2 props = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
		.pNext = &IDProps,
	};

	vkGetPhysicalDeviceProperties2(PhysicalDevice, &props);

	if (IDProps.deviceLUIDValid == VK_FALSE){
		std::array<uint8_t, VK_UUID_SIZE> wrapped;
		std::memcpy(wrapped.data(), IDProps.deviceUUID, VK_UUID_SIZE);
		return UID::FromUUID(wrapped);
	}

	std::array<uint8_t, VK_LUID_SIZE> wrapped;
	std::memcpy(wrapped.data(), IDProps.deviceLUID, VK_LUID_SIZE);
	return UID::FromLUID(wrapped, IDProps.deviceNodeMask);
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
