#pragma once

#include <Image.h>

namespace mz::vk
{

struct mzVulkan_API Stream : SharedFactory<Stream>, DeviceChild
{
    std::vector<rc<Image>> Images;
    std::queue<rc<Image>> Ready;

    std::mutex Mutex;
    std::condition_variable CV;

    CircularIndex<> Head;
    u32 Size;
    ImageCreateInfo Info;
    Stream(Device* Vk, u32 Size, ImageCreateInfo const& info);

    rc<Image> AcquireWrite();
    rc<Image> AcquireRead();

    void ReleaseWrite(rc<Image>);
    void ReleaseRead(rc<Image>);
};

}