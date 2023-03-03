/*
 * Copyright MediaZ AS. All Rights Reserved.
 */

#pragma once

// std
#include <string>
#include <type_traits>

// mzVulkan
#include "Common.h"
#include "vulkan/vulkan_core.h"

namespace mz::vk
{

// struct mzFallbackOptions
// {
//     bool mzDynamicRenderingFallback;
//     bool mzSync2Fallback;
//     bool mzCopy2Fallback;
// };

struct FeatureSet  : VkPhysicalDeviceFeatures2
{
    VkPhysicalDeviceVulkan11Features vk11{};
    VkPhysicalDeviceVulkan12Features vk12{};
    VkPhysicalDeviceVulkan13Features vk13{};
    FeatureSet() : VkPhysicalDeviceFeatures2{} {};
    FeatureSet(VkPhysicalDevice PhysicalDevice)
    {
        vkGetPhysicalDeviceFeatures2(PhysicalDevice, pnext());
    }
    
    FeatureSet& operator=(FeatureSet const& r) { memcpy(this, &r, sizeof(r)); pnext(); return *this;}
    FeatureSet(FeatureSet const& r) { *this = r; }
    FeatureSet operator & (FeatureSet r) const
    {
        for(u32 i = 0; i < sizeof(FeatureSet)/sizeof(VkBool32); ++i) 
            ((VkBool32*)&r)[i] &= ((VkBool32*)this)[i];
        return r;
    }
    
    VkPhysicalDeviceFeatures2* pnext() 
    {
        vk11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        vk12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        vk13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
             sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        vk12.pNext = &vk11;
        vk13.pNext = &vk12;
             pNext = &vk13;
        return this;
    }

};

struct mzVulkan_API Device : SharedFactory<Device>,
                             VklDeviceFunctions
{
    struct mzVulkan_API Global
    {
        u64 Handle;
        void (*Dtor)(Device*, u64);

        Global() : Handle(0), Dtor([](auto,auto) {}) {}

        Global(u64 Handle, void (*Dtor)(Device*, u64)) : Handle(Handle), Dtor(Dtor) {}

        template <class T>
        Global(T* handle)
            : Handle((u64)handle), Dtor([](Device*, u64 handle) { delete (T*)handle; })
        {
        }

        void Free(Device* Dev)
        {
            Dtor(Dev, Handle);
        }

    };

    VkInstance Instance;
    VkPhysicalDevice PhysicalDevice;

    rc<Allocator> ImmAllocator;

    std::map<std::thread::id, rc<CommandPool>> ImmPools;
    std::map<std::thread::id, rc<QueryPool>> ImmQPools;
    rc<CommandPool> GetPool();
    rc<QueryPool> GetQPool();
    
    rc<Queue> Queue;
    FeatureSet Features;
    std::unordered_map<std::string, Global> Globals;

    bool RemoveGlobal(std::string const& id)
    {
        auto it = Globals.find(id);
        if (it != Globals.end())
        {
            it->second.Free(this);
            Globals.erase(it);
            return true;
        }
        return false;
    }

    template <class T>
    T* GetGlobal(std::string const& id)
    {
        if (auto it = Globals.find(id); it != Globals.end())
        {
            return (T*)it->second.Handle;
        }
        return 0;
    }

    template <class T, class... Args>
    requires(std::is_constructible_v<T, Args...>)
        T* RegisterGlobal(std::string const& id, Args&&... args)
    {
        RemoveGlobal(id);
        T* data = new T(std::forward<Args>(args)...);
        Globals[id] = Global(data);
        return data;
    }
    
    Device(VkInstance Instance, VkPhysicalDevice PhysicalDevice);
    ~Device();
    u64 GetLuid() const;

    static bool CheckSupport(VkPhysicalDevice PhysicalDevice);
    std::string GetName() const;

}; // namespace mz::vk

struct mzVulkan_API Context : SharedFactory<Context>
{
    typedef VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);

    void* Lib;

    VkInstance Instance;
    VkDebugUtilsMessengerEXT Msger = 0;
    std::vector<rc<Device>> Devices;

    rc<Device> CreateDevice(u64 luid) const;
    ~Context();
    Context(DebugCallback* = 0);
    void OrderDevices();
    static void EnableValidationLayers(bool enable);
};
} // namespace mz::vk