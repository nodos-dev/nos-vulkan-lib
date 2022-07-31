#include "Stream.h"
#include "Image.h"
#include <algorithm>
#include <chrono>
#include <memory>

namespace mz::vk
{
    
Stream::Stream(Device* Vk, u32 Size, ImageCreateInfo const& info) : 
    DeviceChild(Vk), Head(Size), Size(Size), Info(info)
{
    assert(!info.Imported);
    for(u32 i = 0; i < Size; ++i)
    {
        Images.push_back(MakeShared<Image>(Vk, info));
    }
}

rc<Image> Stream::AcquireWrite()
{
    return Images[Head++];
}

rc<Image> Stream::AcquireRead()
{
    std::unique_lock lock(Mutex);
    if(Ready.empty())
    {
        return Images[Head];
    }
    return Ready.front();
}

void Stream::ReleaseWrite(rc<Image> img)
{
    std::unique_lock lock(Mutex);
    Ready.push(img);
    if (Ready.size() > Size)
    {
        Ready.pop();
    }
}

void Stream::ReleaseRead(rc<Image> img)
{
    // std::unique_lock lock(Mutex);
    // Images.push(img);
}

}