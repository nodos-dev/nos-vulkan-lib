#pragma once

#include <mzVkCommon.h>
#include <string>
#include <type_traits>

namespace mz::vk
{

struct mzFallbackOptions
{
    bool mzDynamicRenderingFallback;
    bool mzSync2Fallback;
    bool mzCopy2Fallback;
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
    rc<CommandPool> ImmCmdPool;
    rc<Queue> Queue;
    mzFallbackOptions FallbackOptions;
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
    
    Device(VkInstance Instance, VkPhysicalDevice PhysicalDevice, mzFallbackOptions FallbackOptions);
    ~Device();
    u64 GetLuid() const;

    static bool IsSupported(VkPhysicalDevice PhysicalDevice);
    static bool GetFallbackOptionsForDevice(VkPhysicalDevice PhysicalDevice, mzFallbackOptions& FallbackOptions);
    
    std::string GetName() const;

}; // namespace mz::vk

struct mzVulkan_API Context : SharedFactory<Context>
{
    void* Lib;

    VkInstance Instance;

    std::vector<rc<Device>> Devices;

    rc<Device> CreateDevice(u64 luid) const;
    void OrderDevices();
    ~Context();
    Context();
};
} // namespace mz::vk