#include "Buffer.h"


void Buffer::unmapped(VmaAllocator allocator, vector<pair<u64, u8*>> raws, VkBufferUsageFlags usage)
{
    u64 total = 0;
    for (auto [s, _] : raws)
    {
        total += s;
    }

    create(allocator, total, usage | Buffer::Dst, Buffer::Unmapped);

    Buffer staging;
    staging.create(allocator, total, Buffer::Src, Buffer::Mapped);

    u64 offset = 0;

    for (auto [size, data] : raws)
    {
        memcpy(staging.mapping + offset, data, size);
        offset += size;
    }

    // res.execute([src = staging.handle, dst = handle, total](VkCommandBuffer cmd) {
    //         VkBufferCopy reg = {0, 0, total};
    //         vkCmdCopyBuffer(cmd, src, dst, 1, &reg); },
    //             [staging = staging, allocator = res.allocator]() { vmaDestroyBuffer(allocator, staging.handle, staging.allocation); });
}
