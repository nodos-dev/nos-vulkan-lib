#pragma once

#include <mzVkCommon.h>

namespace mz::vk
{
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

    std::unordered_map<std::string, Global> Globals;

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
        T* RegisterGlobal(std::string id, Args&&... args)
    {
        T* data = new T(std::forward<Args>(args)...);

        auto it = Globals.find(id);

        if (it != Globals.end())
        {
            it->second.Free(this);
            Globals.erase(it);
        }

        Globals.insert(it, std::make_pair(std::move(id), Global(data)));
        return data;
    }

    Device(VkInstance Instance, VkPhysicalDevice PhysicalDevice);
    ~Device();

    u64 GetLuid() const;

}; // namespace mz::vk

struct mzVulkan_API Context : SharedFactory<Context>
{
    void* Lib;

    VkInstance Instance;

    std::vector<rc<Device>> Devices;

    rc<Device> CreateDevice(u64 luid) const;
    ~Context();
    Context();
};
} // namespace mz::vk