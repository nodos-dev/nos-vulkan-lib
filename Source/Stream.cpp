// Copyright MediaZ Teknoloji A.S. All Rights Reserved.


// std
#include <algorithm>
#include <chrono>
#include <memory>

// nosVulkan
#include "nosVulkan/Stream.h"
#include "nosVulkan/Image.h"

namespace nos::vk
{

Stream::Resource::Resource(u32 idx, Device* Vk, ImageCreateInfo const& info) : idx(idx), Image(Vk, info) { }

Stream::Stream(Device* Vk, u32 Size, ImageCreateInfo const& info) : Head(Size), Tail(Size), Size(Size),  Info(info)
{
    for(u32 i = 0; i < Size; ++i)
    {
        Pool.push_back(MakeShared<Resource>(i, Vk, info));
    }
}

u32 Stream::InUse() const
{
    return Head.Val - Tail.Val;
}

rc<Image> Stream::AcquireWrite()
{
    std::unique_lock lock(Mutex);
    while (InUse() == Size || Pool[Head]->read)
    {
        WCV.wait(lock);
    }

    NOS_ASSERT(!Pool[Head]->written);
    NOS_ASSERT(!Pool[Head]->read);
    Pool[Head]->written = true;
    return Pool[Head++];
}

void Stream::ReleaseWrite(rc<Image> Image)
{
    Resource* img = (Resource*)Image.get();
    std::unique_lock lock(Mutex);
    NOS_ASSERT(img->written);
    NOS_ASSERT(!img->read);
    img->written = false;
    RCV.notify_one();
}

rc<Image> Stream::AcquireRead()
{
    std::unique_lock lock(Mutex);
    while(InUse() == 0 || Pool[Tail]->written)
    {
        RCV.wait(lock);
    }
    NOS_ASSERT(!Pool[Tail]->written);
    NOS_ASSERT(!Pool[Tail]->read);
    Pool[Tail]->read = true;
    return Pool[Tail++];
}

void Stream::ReleaseRead(rc<Image> Image)
{
    Resource* img = (Resource*)Image.get();
    std::unique_lock lock(Mutex);
    NOS_ASSERT(img->read);
    NOS_ASSERT(!img->written);
    img->read = false;
    WCV.notify_one();
}

}