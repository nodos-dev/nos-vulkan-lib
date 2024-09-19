/*
 * Copyright MediaZ Teknoloji A.S. All Rights Reserved.
 */

#pragma once

#include "Image.h"
#include <condition_variable>
#include <atomic>

namespace nos::vk
{

// struct nosVulkan_API Stream : SharedFactory<Stream>, DeviceChild
// {
//     std::vector<rc<Image>> Images;
//     std::queue<rc<Image>> Ready;

//     std::mutex Mutex;
//     std::condition_variable CV;

//     CircularIndex<> Head;
//     u32 Size;
//     ImageCreateInfo Info;
//     Stream(Device* Vk, u32 Size, ImageCreateInfo const& info);

//     rc<Image> AcquireWrite();
//     rc<Image> AcquireRead();

//     void ReleaseWrite(rc<Image>);
//     void ReleaseRead(rc<Image>);
// };

struct nosVulkan_API Stream : SharedFactory<Stream>, DeviceChild
{
    struct Resource: Image
    {
        Resource(u32 idx, Device* Vk, ImageCreateInfo const& info);
        const u32 idx;
        std::atomic_bool written = false;
        std::atomic_bool read = false;
    };
    ImageCreateInfo Info;
    std::vector<rc<Resource>> Pool;

    CircularIndex<std::atomic_uint> Head;
    CircularIndex<std::atomic_uint> Tail;
    const u32 Size = 0;
    
    std::mutex Mutex;
    std::condition_variable WCV;
    std::condition_variable RCV;

    Stream(Device* Vk, u32 Size, ImageCreateInfo const& info);
    u32 InUse() const;
    rc<Image> AcquireWrite();
    void ReleaseWrite(rc<Image> img);
    rc<Image> AcquireRead();
    void ReleaseRead(rc<Image> img);
};

}