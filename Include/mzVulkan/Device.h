/*
 * Copyright MediaZ AS. All Rights Reserved.
 */

#pragma once

// mzVulkan
#include "Common.h"
#include "Allocation.h"

template<>
struct std::hash<VkSamplerCreateInfo>
{
    using Arr = std::array<u8, sizeof(VkSamplerCreateInfo) - offsetof(VkSamplerCreateInfo, flags)>;
    size_t operator()(VkSamplerCreateInfo const& info) const 
    {
        size_t re = 0;
        mz::hash_combine(re, 
            info.flags, 
            info.magFilter, 
            info.minFilter, 
            info.mipmapMode, 
            info.addressModeU, 
            info.addressModeV, 
            info.addressModeW, 
            info.mipLodBias, 
            info.minLod, 
            info.maxLod, 
            info.borderColor, 
            (bool)info.unnormalizedCoordinates);
            if(info.compareEnable) mz::hash_combine(re, info.compareOp);
            if(info.anisotropyEnable) mz::hash_combine(re, info.maxAnisotropy);
        return re;
    }
};

inline bool operator == (VkSamplerCreateInfo const& a, VkSamplerCreateInfo const& b)
{
    return  a.flags == b.flags &&
            a.magFilter == b.magFilter &&
            a.minFilter == b.minFilter &&
            a.mipmapMode == b.mipmapMode &&
            a.addressModeU == b.addressModeU &&
            a.addressModeV == b.addressModeV &&
            a.addressModeW == b.addressModeW &&
            a.mipLodBias == b.mipLodBias &&
            a.minLod == b.minLod &&
            a.maxLod == b.maxLod &&
            a.borderColor == b.borderColor &&
            (bool)a.unnormalizedCoordinates == (bool)b.unnormalizedCoordinates && 
            a.compareEnable == b.compareEnable &&
            a.anisotropyEnable == b.anisotropyEnable &&
            (a.compareEnable ? a.compareOp == b.compareOp : true) &&
            (a.anisotropyEnable ? a.maxAnisotropy == b.maxAnisotropy : true);
}

namespace mz::vk
{

struct FeatureSet  : 
    VkPhysicalDeviceFeatures2,
    VkPhysicalDeviceVulkan13Features, 
    VkPhysicalDeviceVulkan12Features,
    VkPhysicalDeviceVulkan11Features
{
    FeatureSet()  { memset(this, 0, sizeof(*this)); }

    FeatureSet(VkPhysicalDevice PhysicalDevice) : FeatureSet()
    {
        vkGetPhysicalDeviceFeatures2(PhysicalDevice, pnext());
    }
    
    FeatureSet& operator=(FeatureSet const& r) { memcpy(this, &r, sizeof(r)); pnext(); return *this;}
    FeatureSet(FeatureSet const& r) { *this = r; }
    
    FeatureSet operator & (FeatureSet r) const
    {
        for(u32 i = 0; i < sizeof(*this)/sizeof(VkBool32); ++i) 
            ((VkBool32*)&r)[i] &= ((VkBool32*)this)[i];
        return r;
    }
    
    VkPhysicalDeviceFeatures2* pnext()
    {
        VkPhysicalDeviceVulkan11Features::sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        VkPhysicalDeviceVulkan12Features::sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        VkPhysicalDeviceVulkan13Features::sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        VkPhysicalDeviceFeatures2::sType        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        VkPhysicalDeviceVulkan11Features::pNext = nullptr;
        VkPhysicalDeviceVulkan12Features::pNext = static_cast<VkPhysicalDeviceVulkan11Features*>(this);
        VkPhysicalDeviceVulkan13Features::pNext = static_cast<VkPhysicalDeviceVulkan12Features*>(this);
        VkPhysicalDeviceFeatures2::pNext        = static_cast<VkPhysicalDeviceVulkan13Features*>(this);
        return static_cast<VkPhysicalDeviceFeatures2*>(this);
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
            : Handle((u64)handle), Dtor([](Device*, u64 handle) 
            { 
                delete (T*)handle; 
            })
        {
        }

        void Free(Device* Dev)
        {
            Dtor(Dev, Handle);
        }

    };

    VkInstance Instance;
    VkPhysicalDevice PhysicalDevice;

    VmaAllocator Allocator;

    std::map<std::thread::id, std::pair<rc<CommandPool>, rc<QueryPool>>> ImmPools;
	std::shared_mutex ImmPoolsMutex;
    
    rc<CommandPool> GetPool();
    rc<QueryPool> GetQPool();
    
    rc<Queue> Queue;
    FeatureSet Features;
    std::unordered_map<std::string, Global> Globals;
    std::vector<std::function<void()>> Callbacks;
	uint64_t SubmitCount = 0;

    std::unordered_map<VkSamplerCreateInfo, VkSampler> Samplers;

    VkSampler GetSampler(VkSamplerCreateInfo const& info);
    VkSampler GetSampler(VkFilter);

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

    template<class T>
    using Spec = SpecializationOf<T, std::shared_ptr>;

    template<class T> struct InnerType { using Inner = T;  };
    
    template<class T> struct InnerType<rc<T>> 
    { 
        using Inner = std::conditional_t<HasEnabledSharedFromThis<T>, T, rc<T>>;
    };

    template<class...> struct Head { using T = void; };
    template<class H, class...R> struct Head<H,R...> { using T = H; };

    template<class T>
    using Inner = typename InnerType<T>::Inner;

    template<class T> static constexpr bool IS_RC = false;
    template<class T> static constexpr bool IS_RC<rc<T>> = true;

    template<class T>
    static constexpr bool IsRC = IS_RC<T> && HasEnabledSharedFromThis<Inner<T>>;

    template<class T>
    using ReturnType = std::conditional_t<IsRC<T>, T, T*>;

    template <class T>
    auto GetGlobal(std::string const& id) -> ReturnType<T>
    {
        if (auto it = Globals.find(id); it != Globals.end())
        {
            if constexpr (IsRC<T>)
            {
                auto ptr = (Inner<T>*)it->second.Handle;
                return T(ptr->shared_from_this(), ptr);
            }
            else 
                return (T*)it->second.Handle;
        }
        return 0;
    }

    template<class T>
    struct alignas(alignof(T)) ManuallyDestruct
    {
        T& Get() { return *(T*)this; }
        
        template<class...Args>
        void init(Args&&...args)
        {
            new (this) T(std::forward<Args>(args)...);
        }
        u8 data[sizeof(T)] = {};
    };

    template <class T, class... Args>
        requires(
            std::is_constructible_v<T, Args...> ||
            (IsRC<T> && std::is_constructible_v<Inner<T>, Args...>))
    auto RegisterGlobal(std::string const &id, Args&&...args) -> ReturnType<T>
    {
        RemoveGlobal(id);

        if constexpr (IsRC<T>)
        {
            ManuallyDestruct<T> tmp;
            if constexpr ((1 == sizeof...(Args)) && std::is_same_v<std::remove_cvref_t<typename Head<Args...>::T>, T>)
            {
                tmp.init(std::forward<Args>(args)...);
            }
            else
            {
                tmp.init(MakeShared<Inner<T>>(std::forward<Args>(args)...));
            }
            Inner<T>* ptr = tmp.Get().get();
            auto dtor = [](auto, u64 handle) { ((Inner<T>*)handle)->shared_from_this().~shared_ptr(); };
            Globals[id] = Global((u64)ptr, dtor);
            return tmp.Get();
        }
        else
        {
            T *data = new T(std::forward(args)...);
            Globals[id] = Global(data);
            return data;
        }
    }

    Device(VkInstance Instance, VkPhysicalDevice PhysicalDevice);
    ~Device();
    u64 GetLuid() const;

    static bool CheckSupport(VkPhysicalDevice PhysicalDevice);
    std::string GetName() const;

protected:
	void InitializeVMA();
}; // namespace mz::vk

static_assert(Device::IsRC<rc<Device>>);

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