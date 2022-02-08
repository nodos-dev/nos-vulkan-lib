

#include "Common.h"

#include "Pipeline.h"
#include "vulkan/vulkan_core.h"

#include <dynalo/dynalo.hpp>

extern "C" const u8 main_comp[2260];

void* load_lib(const char* lib)
{
    return (void*)dynalo::open(lib);
}

void* get_proc_addr(void* lib, const char* proc)
{
    return (void*)dynalo::get_function<void* (*)()>((dynalo::native::handle)lib, proc);
}

template <u64 N>
struct CyclicIdx
{
    u64 val = 0;

    u64 operator++()
    {
        u64 re = val;
        ++val %= N;
        return re;
    }

    u64 operator++(int)
    {
        return ++val %= N;
    }
};

template <class T, int N>
struct Ring
{
    T            Elements[N];
    CyclicIdx<N> Current;

    T& next()
    {
        return Elements[Current++];
    }
};

struct VulkanContext
{
    Pipeline pipeline;

    VkInstance       instance;
    VkDevice         device;
    VkPhysicalDevice physical_device;

    VkQueue render_queue;
    VkQueue compute_queue;
    VkQueue transfer_queue;

    VkCommandPool cmd_pool;

    Ring<std::pair<VkCommandBuffer, VkFence>, 16> cmd_buffers;

    Ring<VkDescriptorSet, 16> desc_sets;

    VulkanContext()
    {
        vkl_init(load_lib, get_proc_addr);

        struct
        {
            u32 family;
            u32 idx;
        } locations[16];

        u32 count;

        vector<const char*> layers = {"VK_LAYER_KHRONOS_validation"};

        vector<const char*> device_ext = {
            "VK_KHR_external_semaphore_win32",
            "VK_KHR_external_memory_win32",
            "VK_EXT_external_memory_host",
        };

        VkApplicationInfo app = {
            .sType      = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .apiVersion = VK_API_VERSION_1_2,
        };

        VkInstanceCreateInfo info = {
            .sType               = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo    = &app,
            .enabledLayerCount   = (u32)layers.size(),
            .ppEnabledLayerNames = layers.data(),
        };

        CHECKRE(vkCreateInstance(&info, 0, &instance));

        vkl_load_instance_functions(instance);

        CHECKRE(vkEnumeratePhysicalDevices(instance, &count, 0));

        vector<VkPhysicalDevice> buff(count);

        CHECKRE(vkEnumeratePhysicalDevices(instance, &count, buff.data()));

        physical_device = buff.front();

        // device
        {

            vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, 0);
            vector<VkQueueFamilyProperties> props(count);
            vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, props.data());

            vector<VkDeviceQueueCreateInfo> queue_infos;

            VkQueueFlagBits queues[] = {
                VK_QUEUE_GRAPHICS_BIT,
                VK_QUEUE_COMPUTE_BIT,
                VK_QUEUE_TRANSFER_BIT,
            };

            bool found_queues[16] = {};
            // pair<u32, u32> locations[16]    = {};

            struct
            {
                float f = 1.f;
            } prio[16] = {};

            for (u32 i = 0; i < props.size(); ++i)
            {
                VkQueueFamilyProperties prop = props[i];
                VkDeviceQueueCreateInfo info = {
                    .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .queueFamilyIndex = i,
                    .pQueuePriorities = &prio->f,
                };

                for (auto& q : queues)
                {
                    if (!found_queues[q] && (prop.queueFlags && q))
                    {
                        locations[q].family = i;
                        locations[q].idx    = info.queueCount++;
                        found_queues[q]     = true;
                    }
                }

                if (info.queueCount)
                {
                    queue_infos.push_back(info);
                }
            }

            VkDeviceCreateInfo info = {
                .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                .queueCreateInfoCount    = (u32)queue_infos.size(),
                .pQueueCreateInfos       = queue_infos.data(),
                .enabledLayerCount       = (u32)layers.size(),
                .ppEnabledLayerNames     = layers.data(),
                .enabledExtensionCount   = (u32)device_ext.size(),
                .ppEnabledExtensionNames = device_ext.data(),
            };

            CHECKRE(vkCreateDevice(physical_device, &info, 0, &device));

            vkl_load_device_functions(device);

            vkGetDeviceQueue(device, locations[VK_QUEUE_GRAPHICS_BIT].family, locations[VK_QUEUE_GRAPHICS_BIT].idx, &render_queue);
            vkGetDeviceQueue(device, locations[VK_QUEUE_COMPUTE_BIT].family, locations[VK_QUEUE_COMPUTE_BIT].idx, &compute_queue);
            vkGetDeviceQueue(device, locations[VK_QUEUE_TRANSFER_BIT].family, locations[VK_QUEUE_TRANSFER_BIT].idx, &transfer_queue);

            VkCommandPoolCreateInfo cmd_pool_info = {
                .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = locations[VK_QUEUE_GRAPHICS_BIT].family,
            };

            CHECKRE(vkCreateCommandPool(device, &cmd_pool_info, 0, &cmd_pool));
        }

        pipeline.create(device, {{(u32*)main_comp, (u32*)(main_comp + 2260)}}, 0);

        pipeline.layout.AllocateSets(device, 0, 16, desc_sets.Elements);
    }

    void exec(VkImageView src_handle, VkImageView dst_handle)
    {
        auto [cmd, fence] = cmd_buffers.next();
        auto dset         = desc_sets.next();

        {
            CHECKRE(vkWaitForFences(device, 1, &fence, 1, -1));
            CHECKRE(vkResetFences(device, 1, &fence));
            VkCommandBufferBeginInfo info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            };
            CHECKRE(vkBeginCommandBuffer(cmd, &info));
        }

        {
            VkDescriptorImageInfo info[2] = {
                {
                    .imageView   = src_handle,
                    .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                },
                {
                    .imageView   = dst_handle,
                    .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                }};

            VkWriteDescriptorSet write[2] = {{
                                                 .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                                 .dstSet          = dset,
                                                 .dstBinding      = 0,
                                                 .descriptorCount = 1,
                                                 .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                 .pImageInfo      = &info[0],
                                             },
                                             {
                                                 .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                                 .dstSet          = dset,
                                                 .dstBinding      = 1,
                                                 .descriptorCount = 1,
                                                 .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                 .pImageInfo      = &info[1],
                                             }};

            vkUpdateDescriptorSets(device, 2, write, 0, 0);
        }

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.handle);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.layout.handle, 0, 1, &dset, 0, 0);
        vkCmdDispatch(cmd, 1920, 1080, 1);

        {
            CHECKRE(vkEndCommandBuffer(cmd));

            u32 stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

            VkSubmitInfo info = {
                .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .pWaitDstStageMask  = &stage,
                .commandBufferCount = 1,
                .pCommandBuffers    = &cmd,
            };

            CHECKRE(vkQueueSubmit(compute_queue, 1, &info, fence));
        }
    }
};

void ExecDummyPass(void* src, void* dst)
{
    static VulkanContext vk;
}
