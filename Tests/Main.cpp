#include <mzVulkan/Buffer.h>
#include <mzVulkan/Common.h>
#include <mzVulkan/Device.h>
#include <mzVulkan/Image.h>

#include <iostream>

int main() {
  auto vkCtx = mz::vk::Context::New();
  if (vkCtx->Devices.empty()) {
    return 1;
  }
  auto vkDevice = vkCtx->Devices[0].get();
  std::cout << "Device: " << vkDevice->GetName() << std::endl;

  return 0;
}