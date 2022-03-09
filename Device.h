#pragma once

#include "mzVkCommon.h"

namespace mz::vk
{
struct Device : SharedFactory<Device>,
                      VklDeviceFunctions
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

    std::shared_ptr<struct Allocator> ImmAllocator;
    std::shared_ptr<struct CommandPool>     ImmCmdPool;

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

    Device(VkInstance                      Instance,
                 VkPhysicalDevice                PhysicalDevice,
                 std::vector<const char*> const& layers,
                 std::vector<const char*> const& extensions);
    ~Device();

    u64 GetLuid()
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

}; // namespace mz::vk

struct Context : SharedFactory<Context>
{
    void* lib;

    VkInstance Instance;

    std::vector<std::shared_ptr<Device>> Devices;

    ~Context();
    Context();
};
} // namespace mz::vk