#pragma once

#include "mzVkCommon.h"

struct VulkanDevice : std::enable_shared_from_this<VulkanDevice>,
                      VklDeviceFunctions,
                      Uncopyable
{
    struct Global
    {
        u64 handle;
        void (*dtor)(u64);

        template <class T>
        Global(T* handle)
            : handle((u64)handle), dtor([](u64 handle) { delete (T*)handle; })
        {
        }

        void Free()
        {
            dtor(handle);
        }
    };

    VkInstance       Instance;
    VkPhysicalDevice PhysicalDevice;

    u32 QueueFamily;

    std::unordered_map<std::string, Global> Globals;

    template <class T>
    T* GetGlobal(std::string const& id)
    {
        if (auto it = Globals.find(id); it != Globals.end())
        {
            return (T*)it->second.handle;
        }
        return 0;
    }

    template <class T, class... Args>
    T* RegisterGlobal(std::string id, Args&&... args)
    {
        T* data = new T(std::forward<Args>(args)...);

        auto it = Globals.find(id);

        if (it != Globals.end())
        {
            it->second.Free();
            Globals.erase(it);
        }

        Globals.insert(it, std::make_pair(std::move(id), Global(data)));
        return data;
    }

    VulkanDevice()                    = delete;
    VulkanDevice(VulkanDevice const&) = delete;

    VulkanDevice(VkInstance                      Instance,
                 VkPhysicalDevice                PhysicalDevice,
                 std::vector<const char*> const& layers,
                 std::vector<const char*> const& extensions);
    ~VulkanDevice();
};

struct VulkanContext : std::enable_shared_from_this<VulkanContext>, Uncopyable
{
    void* lib;

    VkInstance Instance;

    std::vector<std::shared_ptr<VulkanDevice>> Devices;

    ~VulkanContext();
    VulkanContext();
};
