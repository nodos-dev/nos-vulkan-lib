#include <nosVulkan/Buffer.h>
#include <nosVulkan/Common.h>
#include <nosVulkan/Device.h>
#include <nosVulkan/Image.h>

#include <iostream>

int main() {
  auto vkCtx = nos::vk::Context::New();
  if (vkCtx->Devices.empty()) {
    return 1;
  }
  auto vkDevice = vkCtx->Devices[0].get();
  std::cout << "Device: " << vkDevice->GetName() << std::endl;

  return 0;
}