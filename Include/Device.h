#pragma once

#include <mzVkCommon.h>
#include <type_traits>

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

        template<class T>
        auto Get() 
        {
            if constexpr(std::is_base_of_v<SharedFactory<T>, T>)
                 return ((T*)Handle)->shared_from_this();
            else return ((T*)Handle);
        }
    };

    VkInstance Instance;
    VkPhysicalDevice PhysicalDevice;
    rc<Allocator> ImmAllocator;
    rc<CommandPool> ImmCmdPool;
    rc<Queue> Queue;

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
    auto GetGlobal(std::string const& id)
    {
        if (auto it = Globals.find(id); it != Globals.end())
        {
            return it->second.Get<T>();
        }
        return decltype(Global().Get<T>())(0);
    }

    template<class T>
    static void AddRef(rc<T> val)
    {
        char buf[sizeof(rc<T>)] = {};
        new (buf) rc<T>(val); // dirty way to addref
    }

    template<class T>
    static void DecRef(rc<T> val)
    {
        val.~rc<T>();
    }

    template <class T, class... Args>
    requires(std::is_constructible_v<T, Args...>)
        auto RegisterGlobal(std::string const& id, Args&&... args)
    {
        RemoveGlobal(id);
        if constexpr(std::is_base_of_v<SharedFactory<T>, T>)
        {
            auto shared = MakeShared<T>(std::forward<Args>(args)...);
            AddRef<T>(shared);
            Globals[id] = Global((u64)shared.get(), [](Device*, u64 handle) { DecRef<T>(((T*)handle)->shared_from_this()); });
            return shared;
        }
        else 
        {
            T* data = new T(std::forward<Args>(args)...);
            Globals[id] = Global(data);
            return data;
        }
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