#include "framework.h"
#include "graphics.h"

#define VK_USE_PLATFORM_WIN32_KHR
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#define MAG_VK_FRAMES_IN_FLIGHT 2

typedef struct MAGVKFRAME
{
  VkCommandBuffer commandBuffer;
  VkSemaphore     imageAvailable;
  VkSemaphore     renderFinished;
  VkFence         fence;
  VkBuffer        uploadBuffer;
  VkDeviceMemory  uploadMemory;
  BYTE*           mappedUpload;
} MAGVKFRAME;

typedef struct MAGVULKANSTATE
{
  HMODULE          module;
  VkInstance       instance;
  VkSurfaceKHR     surface;
  VkPhysicalDevice physicalDevice;
  VkDevice         device;
  VkQueue          queue;
  UINT             queueFamily;
  VkSwapchainKHR   swapchain;
  VkFormat         swapchainFormat;
  VkExtent2D       extent;
  VkImage*         images;
  VkBool32*        imageInitialized;
  VkFence*         imageFences;
  UINT             imageCount;
  VkCommandPool    commandPool;
  MAGVKFRAME       frames[MAG_VK_FRAMES_IN_FLIGHT];
  UINT             currentFrame;
  MAGCPUCOMPOSITOR compositor;

  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
  PFN_vkCreateInstance vkCreateInstance;
  PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties;
  PFN_vkDestroyInstance vkDestroyInstance;
  PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR;
  PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR;
  PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
  PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties;
  PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR;
  PFN_vkEnumerateDeviceExtensionProperties vkEnumerateDeviceExtensionProperties;
  PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
  PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR;
  PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR;
  PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties;
  PFN_vkCreateDevice vkCreateDevice;
  PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr;
  PFN_vkDestroyDevice vkDestroyDevice;
  PFN_vkGetDeviceQueue vkGetDeviceQueue;
  PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR;
  PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR;
  PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
  PFN_vkCreateCommandPool vkCreateCommandPool;
  PFN_vkDestroyCommandPool vkDestroyCommandPool;
  PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers;
  PFN_vkCreateSemaphore vkCreateSemaphore;
  PFN_vkDestroySemaphore vkDestroySemaphore;
  PFN_vkCreateFence vkCreateFence;
  PFN_vkDestroyFence vkDestroyFence;
  PFN_vkWaitForFences vkWaitForFences;
  PFN_vkResetFences vkResetFences;
  PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR;
  PFN_vkResetCommandBuffer vkResetCommandBuffer;
  PFN_vkBeginCommandBuffer vkBeginCommandBuffer;
  PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier;
  PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImage;
  PFN_vkEndCommandBuffer vkEndCommandBuffer;
  PFN_vkQueueSubmit vkQueueSubmit;
  PFN_vkQueuePresentKHR vkQueuePresentKHR;
  PFN_vkDeviceWaitIdle vkDeviceWaitIdle;
  PFN_vkCreateBuffer vkCreateBuffer;
  PFN_vkDestroyBuffer vkDestroyBuffer;
  PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
  PFN_vkAllocateMemory vkAllocateMemory;
  PFN_vkFreeMemory vkFreeMemory;
  PFN_vkBindBufferMemory vkBindBufferMemory;
  PFN_vkMapMemory vkMapMemory;
  PFN_vkUnmapMemory vkUnmapMemory;
} MAGVULKANSTATE;

#define MAG_VK_LOAD_GLOBAL(State, Name) \
  ((State)->Name = (PFN_##Name)(State)->vkGetInstanceProcAddr(VK_NULL_HANDLE, #Name))
#define MAG_VK_LOAD_INSTANCE(State, Name) \
  ((State)->Name = (PFN_##Name)(State)->vkGetInstanceProcAddr((State)->instance, #Name))
#define MAG_VK_LOAD_DEVICE(State, Name) \
  ((State)->Name = (PFN_##Name)(State)->vkGetDeviceProcAddr((State)->device, #Name))

static BOOL magGraphicsVulkanIsAvailable(LPTSTR reason, UINT reasonCount)
{
    HMODULE module = LoadLibrary(TEXT("vulkan-1.dll"));
    BOOL available = FALSE;

    if (module)
    {
      available = NULL != GetProcAddress(module, "vkGetInstanceProcAddr");
      FreeLibrary(module);
    }

    if (reason && reasonCount)
    {
      if (available)
      {
        reason[0] = TEXT('\0');
      }
      else
      {
        lstrcpyn(reason, TEXT("The Vulkan loader is not installed."), reasonCount);
      }
    }
    return available;
}

static BOOL magGraphicsVulkanLoadGlobalFunctions(MAGVULKANSTATE* state)
{
    TCHAR layerFilter[2];

    SetLastError(ERROR_SUCCESS);
    if (!GetEnvironmentVariable(
          TEXT("VK_LOADER_LAYERS_DISABLE"),
          layerFilter,
          ARRAYSIZE(layerFilter)) &&
        ERROR_ENVVAR_NOT_FOUND == GetLastError())
    {
      /* Implicit capture/overlay layers are outside MAG's backend contract and
         can retain state from WGL/DXGI presenters across an API switch.  The
         Vulkan loader explicitly supports this application-local safe mode. */
      SetEnvironmentVariable(TEXT("VK_LOADER_LAYERS_DISABLE"), TEXT("~implicit~"));
    }

    state->module = LoadLibrary(TEXT("vulkan-1.dll"));
    if (!state->module)
    {
      return FALSE;
    }

    state->vkGetInstanceProcAddr =
      (PFN_vkGetInstanceProcAddr)GetProcAddress(state->module, "vkGetInstanceProcAddr");
    return state->vkGetInstanceProcAddr &&
      MAG_VK_LOAD_GLOBAL(state, vkCreateInstance) &&
      MAG_VK_LOAD_GLOBAL(state, vkEnumerateInstanceExtensionProperties);
}

static BOOL magGraphicsVulkanLoadInstanceFunctions(MAGVULKANSTATE* state)
{
    return
      MAG_VK_LOAD_INSTANCE(state, vkDestroyInstance) &&
      MAG_VK_LOAD_INSTANCE(state, vkCreateWin32SurfaceKHR) &&
      MAG_VK_LOAD_INSTANCE(state, vkDestroySurfaceKHR) &&
      MAG_VK_LOAD_INSTANCE(state, vkEnumeratePhysicalDevices) &&
      MAG_VK_LOAD_INSTANCE(state, vkGetPhysicalDeviceQueueFamilyProperties) &&
      MAG_VK_LOAD_INSTANCE(state, vkGetPhysicalDeviceSurfaceSupportKHR) &&
      MAG_VK_LOAD_INSTANCE(state, vkEnumerateDeviceExtensionProperties) &&
      MAG_VK_LOAD_INSTANCE(state, vkGetPhysicalDeviceSurfaceCapabilitiesKHR) &&
      MAG_VK_LOAD_INSTANCE(state, vkGetPhysicalDeviceSurfaceFormatsKHR) &&
      MAG_VK_LOAD_INSTANCE(state, vkGetPhysicalDeviceSurfacePresentModesKHR) &&
      MAG_VK_LOAD_INSTANCE(state, vkGetPhysicalDeviceMemoryProperties) &&
      MAG_VK_LOAD_INSTANCE(state, vkCreateDevice) &&
      MAG_VK_LOAD_INSTANCE(state, vkGetDeviceProcAddr);
}

static BOOL magGraphicsVulkanLoadDeviceFunctions(MAGVULKANSTATE* state)
{
    return
      MAG_VK_LOAD_DEVICE(state, vkDestroyDevice) &&
      MAG_VK_LOAD_DEVICE(state, vkGetDeviceQueue) &&
      MAG_VK_LOAD_DEVICE(state, vkCreateSwapchainKHR) &&
      MAG_VK_LOAD_DEVICE(state, vkDestroySwapchainKHR) &&
      MAG_VK_LOAD_DEVICE(state, vkGetSwapchainImagesKHR) &&
      MAG_VK_LOAD_DEVICE(state, vkCreateCommandPool) &&
      MAG_VK_LOAD_DEVICE(state, vkDestroyCommandPool) &&
      MAG_VK_LOAD_DEVICE(state, vkAllocateCommandBuffers) &&
      MAG_VK_LOAD_DEVICE(state, vkCreateSemaphore) &&
      MAG_VK_LOAD_DEVICE(state, vkDestroySemaphore) &&
      MAG_VK_LOAD_DEVICE(state, vkCreateFence) &&
      MAG_VK_LOAD_DEVICE(state, vkDestroyFence) &&
      MAG_VK_LOAD_DEVICE(state, vkWaitForFences) &&
      MAG_VK_LOAD_DEVICE(state, vkResetFences) &&
      MAG_VK_LOAD_DEVICE(state, vkAcquireNextImageKHR) &&
      MAG_VK_LOAD_DEVICE(state, vkResetCommandBuffer) &&
      MAG_VK_LOAD_DEVICE(state, vkBeginCommandBuffer) &&
      MAG_VK_LOAD_DEVICE(state, vkCmdPipelineBarrier) &&
      MAG_VK_LOAD_DEVICE(state, vkCmdCopyBufferToImage) &&
      MAG_VK_LOAD_DEVICE(state, vkEndCommandBuffer) &&
      MAG_VK_LOAD_DEVICE(state, vkQueueSubmit) &&
      MAG_VK_LOAD_DEVICE(state, vkQueuePresentKHR) &&
      MAG_VK_LOAD_DEVICE(state, vkDeviceWaitIdle) &&
      MAG_VK_LOAD_DEVICE(state, vkCreateBuffer) &&
      MAG_VK_LOAD_DEVICE(state, vkDestroyBuffer) &&
      MAG_VK_LOAD_DEVICE(state, vkGetBufferMemoryRequirements) &&
      MAG_VK_LOAD_DEVICE(state, vkAllocateMemory) &&
      MAG_VK_LOAD_DEVICE(state, vkFreeMemory) &&
      MAG_VK_LOAD_DEVICE(state, vkBindBufferMemory) &&
      MAG_VK_LOAD_DEVICE(state, vkMapMemory) &&
      MAG_VK_LOAD_DEVICE(state, vkUnmapMemory);
}

static BOOL magGraphicsVulkanHasDeviceExtension(
  MAGVULKANSTATE* state,
  VkPhysicalDevice physicalDevice,
  LPCSTR extensionName)
{
    VkExtensionProperties* properties;
    UINT count = 0;
    UINT i;
    BOOL found = FALSE;

    if (VK_SUCCESS != state->vkEnumerateDeviceExtensionProperties(
          physicalDevice,
          NULL,
          &count,
          NULL) || !count)
    {
      return FALSE;
    }

    properties = (VkExtensionProperties*)HeapAlloc(
      GetProcessHeap(),
      0,
      (SIZE_T)count * sizeof(*properties));
    if (!properties)
    {
      return FALSE;
    }

    if (VK_SUCCESS == state->vkEnumerateDeviceExtensionProperties(
          physicalDevice,
          NULL,
          &count,
          properties))
    {
      for (i = 0; i < count; ++i)
      {
        if (0 == lstrcmpA(properties[i].extensionName, extensionName))
        {
          found = TRUE;
          break;
        }
      }
    }

    HeapFree(GetProcessHeap(), 0, properties);
    return found;
}

static BOOL magGraphicsVulkanSelectPhysicalDevice(MAGVULKANSTATE* state)
{
    VkPhysicalDevice* devices;
    UINT deviceCount = 0;
    UINT deviceIndex;

    if (VK_SUCCESS != state->vkEnumeratePhysicalDevices(state->instance, &deviceCount, NULL) ||
        !deviceCount)
    {
      return FALSE;
    }

    devices = (VkPhysicalDevice*)HeapAlloc(
      GetProcessHeap(),
      0,
      (SIZE_T)deviceCount * sizeof(*devices));
    if (!devices)
    {
      return FALSE;
    }
    if (VK_SUCCESS != state->vkEnumeratePhysicalDevices(state->instance, &deviceCount, devices))
    {
      HeapFree(GetProcessHeap(), 0, devices);
      return FALSE;
    }

    for (deviceIndex = 0; deviceIndex < deviceCount; ++deviceIndex)
    {
      VkQueueFamilyProperties* queues;
      UINT queueCount = 0;
      UINT queueIndex;

      if (!magGraphicsVulkanHasDeviceExtension(
            state,
            devices[deviceIndex],
            VK_KHR_SWAPCHAIN_EXTENSION_NAME))
      {
        continue;
      }

      state->vkGetPhysicalDeviceQueueFamilyProperties(
        devices[deviceIndex],
        &queueCount,
        NULL);
      if (!queueCount)
      {
        continue;
      }

      queues = (VkQueueFamilyProperties*)HeapAlloc(
        GetProcessHeap(),
        0,
        (SIZE_T)queueCount * sizeof(*queues));
      if (!queues)
      {
        continue;
      }
      state->vkGetPhysicalDeviceQueueFamilyProperties(
        devices[deviceIndex],
        &queueCount,
        queues);

      for (queueIndex = 0; queueIndex < queueCount; ++queueIndex)
      {
        VkBool32 presentSupported = VK_FALSE;

        if ((queues[queueIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            VK_SUCCESS == state->vkGetPhysicalDeviceSurfaceSupportKHR(
              devices[deviceIndex],
              queueIndex,
              state->surface,
              &presentSupported) &&
            presentSupported)
        {
          state->physicalDevice = devices[deviceIndex];
          state->queueFamily = queueIndex;
          HeapFree(GetProcessHeap(), 0, queues);
          HeapFree(GetProcessHeap(), 0, devices);
          return TRUE;
        }
      }

      HeapFree(GetProcessHeap(), 0, queues);
    }

    HeapFree(GetProcessHeap(), 0, devices);
    return FALSE;
}

static BOOL magGraphicsVulkanCreateDevice(MAGVULKANSTATE* state)
{
    const FLOAT priority = 1.0f;
    const LPCSTR extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceQueueCreateInfo queueInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    VkDeviceCreateInfo deviceInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };

    queueInfo.queueFamilyIndex = state->queueFamily;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.enabledExtensionCount = ARRAYSIZE(extensions);
    deviceInfo.ppEnabledExtensionNames = extensions;

    if (VK_SUCCESS != state->vkCreateDevice(
          state->physicalDevice,
          &deviceInfo,
          NULL,
          &state->device) ||
        !magGraphicsVulkanLoadDeviceFunctions(state))
    {
      return FALSE;
    }

    state->vkGetDeviceQueue(state->device, state->queueFamily, 0, &state->queue);
    return VK_NULL_HANDLE != state->queue;
}

static UINT magGraphicsVulkanFindMemoryType(
  MAGVULKANSTATE* state,
  UINT typeBits,
  VkMemoryPropertyFlags requiredFlags)
{
    VkPhysicalDeviceMemoryProperties properties;
    UINT i;

    state->vkGetPhysicalDeviceMemoryProperties(state->physicalDevice, &properties);
    for (i = 0; i < properties.memoryTypeCount; ++i)
    {
      if ((typeBits & (1U << i)) &&
          (properties.memoryTypes[i].propertyFlags & requiredFlags) == requiredFlags)
      {
        return i;
      }
    }
    return MAXUINT;
}

static BOOL magGraphicsVulkanCreateUploadBuffer(
  MAGVULKANSTATE* state,
  MAGVKFRAME* frame,
  VkDeviceSize size)
{
    VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    VkMemoryRequirements requirements;
    VkMemoryAllocateInfo allocationInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    UINT memoryType;

    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (VK_SUCCESS != state->vkCreateBuffer(state->device, &bufferInfo, NULL, &frame->uploadBuffer))
    {
      return FALSE;
    }

    state->vkGetBufferMemoryRequirements(state->device, frame->uploadBuffer, &requirements);
    memoryType = magGraphicsVulkanFindMemoryType(
      state,
      requirements.memoryTypeBits,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (MAXUINT == memoryType)
    {
      return FALSE;
    }

    allocationInfo.allocationSize = requirements.size;
    allocationInfo.memoryTypeIndex = memoryType;
    if (VK_SUCCESS != state->vkAllocateMemory(
          state->device,
          &allocationInfo,
          NULL,
          &frame->uploadMemory) ||
        VK_SUCCESS != state->vkBindBufferMemory(
          state->device,
          frame->uploadBuffer,
          frame->uploadMemory,
          0) ||
        VK_SUCCESS != state->vkMapMemory(
          state->device,
          frame->uploadMemory,
          0,
          size,
          0,
          (void**)&frame->mappedUpload))
    {
      return FALSE;
    }
    return TRUE;
}

static void magGraphicsVulkanDestroySwapchainResources(MAGVULKANSTATE* state)
{
    UINT i;

    if (!state->device)
    {
      return;
    }

    for (i = 0; i < MAG_VK_FRAMES_IN_FLIGHT; ++i)
    {
      if (state->frames[i].uploadMemory && state->frames[i].mappedUpload)
      {
        state->vkUnmapMemory(state->device, state->frames[i].uploadMemory);
        state->frames[i].mappedUpload = NULL;
      }
      if (state->frames[i].uploadBuffer)
      {
        state->vkDestroyBuffer(state->device, state->frames[i].uploadBuffer, NULL);
        state->frames[i].uploadBuffer = VK_NULL_HANDLE;
      }
      if (state->frames[i].uploadMemory)
      {
        state->vkFreeMemory(state->device, state->frames[i].uploadMemory, NULL);
        state->frames[i].uploadMemory = VK_NULL_HANDLE;
      }
    }

    if (state->swapchain)
    {
      state->vkDestroySwapchainKHR(state->device, state->swapchain, NULL);
      state->swapchain = VK_NULL_HANDLE;
    }
    if (state->images)
    {
      HeapFree(GetProcessHeap(), 0, state->images);
      state->images = NULL;
    }
    if (state->imageInitialized)
    {
      HeapFree(GetProcessHeap(), 0, state->imageInitialized);
      state->imageInitialized = NULL;
    }
    if (state->imageFences)
    {
      HeapFree(GetProcessHeap(), 0, state->imageFences);
      state->imageFences = NULL;
    }
    state->imageCount = 0;
    ZeroMemory(&state->extent, sizeof(state->extent));
}

static BOOL magGraphicsVulkanCreateSwapchainResources(
  MAGVULKANSTATE* state,
  SIZE clientSize)
{
    VkSurfaceCapabilitiesKHR caps;
    VkSurfaceFormatKHR* formats = NULL;
    VkSurfaceFormatKHR surfaceFormat;
    VkSwapchainCreateInfoKHR swapInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    UINT formatCount = 0;
    UINT imageCount;
    UINT i;
    BOOL foundBgra = FALSE;
    VkDeviceSize uploadSize;

    if (VK_SUCCESS != state->vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
          state->physicalDevice,
          state->surface,
          &caps) ||
        !(caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) ||
        VK_SUCCESS != state->vkGetPhysicalDeviceSurfaceFormatsKHR(
          state->physicalDevice,
          state->surface,
          &formatCount,
          NULL) ||
        !formatCount)
    {
      return FALSE;
    }

    formats = (VkSurfaceFormatKHR*)HeapAlloc(
      GetProcessHeap(),
      0,
      (SIZE_T)formatCount * sizeof(*formats));
    if (!formats ||
        VK_SUCCESS != state->vkGetPhysicalDeviceSurfaceFormatsKHR(
          state->physicalDevice,
          state->surface,
          &formatCount,
          formats))
    {
      if (formats)
      {
        HeapFree(GetProcessHeap(), 0, formats);
      }
      return FALSE;
    }

    surfaceFormat = formats[0];
    if (VK_FORMAT_UNDEFINED == surfaceFormat.format)
    {
      surfaceFormat.format = VK_FORMAT_B8G8R8A8_UNORM;
      surfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
      foundBgra = TRUE;
    }
    else
    {
      for (i = 0; i < formatCount; ++i)
      {
        if (VK_FORMAT_B8G8R8A8_UNORM == formats[i].format &&
            VK_COLOR_SPACE_SRGB_NONLINEAR_KHR == formats[i].colorSpace)
        {
          surfaceFormat = formats[i];
          foundBgra = TRUE;
          break;
        }
      }
    }
    HeapFree(GetProcessHeap(), 0, formats);
    if (!foundBgra)
    {
      return FALSE;
    }

    if (UINT32_MAX != caps.currentExtent.width)
    {
      state->extent = caps.currentExtent;
    }
    else
    {
      state->extent.width = CLAMP((UINT)clientSize.cx, caps.minImageExtent.width, caps.maxImageExtent.width);
      state->extent.height = CLAMP((UINT)clientSize.cy, caps.minImageExtent.height, caps.maxImageExtent.height);
    }
    if (!state->extent.width || !state->extent.height)
    {
      return FALSE;
    }

    imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount && imageCount > caps.maxImageCount)
    {
      imageCount = caps.maxImageCount;
    }

    if (!(caps.supportedCompositeAlpha & compositeAlpha))
    {
      const VkCompositeAlphaFlagBitsKHR candidates[] =
      {
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
      };
      for (i = 0; i < ARRAYSIZE(candidates); ++i)
      {
        if (caps.supportedCompositeAlpha & candidates[i])
        {
          compositeAlpha = candidates[i];
          break;
        }
      }
    }

    swapInfo.surface = state->surface;
    swapInfo.minImageCount = imageCount;
    swapInfo.imageFormat = surfaceFormat.format;
    swapInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapInfo.imageExtent = state->extent;
    swapInfo.imageArrayLayers = 1;
    swapInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapInfo.preTransform = caps.currentTransform;
    swapInfo.compositeAlpha = compositeAlpha;
    swapInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapInfo.clipped = VK_TRUE;
    if (VK_SUCCESS != state->vkCreateSwapchainKHR(
          state->device,
          &swapInfo,
          NULL,
          &state->swapchain))
    {
      return FALSE;
    }
    state->swapchainFormat = surfaceFormat.format;

    if (VK_SUCCESS != state->vkGetSwapchainImagesKHR(
          state->device,
          state->swapchain,
          &state->imageCount,
          NULL) || !state->imageCount)
    {
      magGraphicsVulkanDestroySwapchainResources(state);
      return FALSE;
    }

    state->images = (VkImage*)HeapAlloc(
      GetProcessHeap(),
      0,
      (SIZE_T)state->imageCount * sizeof(*state->images));
    state->imageInitialized = (VkBool32*)HeapAlloc(
      GetProcessHeap(),
      HEAP_ZERO_MEMORY,
      (SIZE_T)state->imageCount * sizeof(*state->imageInitialized));
    state->imageFences = (VkFence*)HeapAlloc(
      GetProcessHeap(),
      HEAP_ZERO_MEMORY,
      (SIZE_T)state->imageCount * sizeof(*state->imageFences));
    if (!state->images || !state->imageInitialized || !state->imageFences ||
        VK_SUCCESS != state->vkGetSwapchainImagesKHR(
          state->device,
          state->swapchain,
          &state->imageCount,
          state->images))
    {
      magGraphicsVulkanDestroySwapchainResources(state);
      return FALSE;
    }

    uploadSize = (VkDeviceSize)state->extent.width * state->extent.height * 4U;
    for (i = 0; i < MAG_VK_FRAMES_IN_FLIGHT; ++i)
    {
      if (!magGraphicsVulkanCreateUploadBuffer(state, &state->frames[i], uploadSize))
      {
        magGraphicsVulkanDestroySwapchainResources(state);
        return FALSE;
      }
    }
    return TRUE;
}

static BOOL magGraphicsVulkanCreateSyncResources(MAGVULKANSTATE* state)
{
    VkCommandPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    VkCommandBufferAllocateInfo commandInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    VkCommandBuffer commandBuffers[MAG_VK_FRAMES_IN_FLIGHT];
    VkSemaphoreCreateInfo semaphoreInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fenceInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    UINT i;

    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = state->queueFamily;
    if (VK_SUCCESS != state->vkCreateCommandPool(
          state->device,
          &poolInfo,
          NULL,
          &state->commandPool))
    {
      return FALSE;
    }

    commandInfo.commandPool = state->commandPool;
    commandInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandInfo.commandBufferCount = MAG_VK_FRAMES_IN_FLIGHT;
    if (VK_SUCCESS != state->vkAllocateCommandBuffers(
          state->device,
          &commandInfo,
          commandBuffers))
    {
      return FALSE;
    }

    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (i = 0; i < MAG_VK_FRAMES_IN_FLIGHT; ++i)
    {
      state->frames[i].commandBuffer = commandBuffers[i];
      if (VK_SUCCESS != state->vkCreateSemaphore(
            state->device,
            &semaphoreInfo,
            NULL,
            &state->frames[i].imageAvailable) ||
          VK_SUCCESS != state->vkCreateSemaphore(
            state->device,
            &semaphoreInfo,
            NULL,
            &state->frames[i].renderFinished) ||
          VK_SUCCESS != state->vkCreateFence(
            state->device,
            &fenceInfo,
            NULL,
            &state->frames[i].fence))
      {
        return FALSE;
      }
    }
    return TRUE;
}

static void magGraphicsVulkanDestroy(HWND hWnd, void* opaqueState);

static BOOL magGraphicsVulkanCreate(HWND hWnd, SIZE clientSize, void** stateOut)
{
    MAGVULKANSTATE* state;
    const LPCSTR extensions[] =
    {
      VK_KHR_SURFACE_EXTENSION_NAME,
      VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
    };
    VkApplicationInfo applicationInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    VkInstanceCreateInfo instanceInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    VkWin32SurfaceCreateInfoKHR surfaceInfo = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };

    if (!stateOut || clientSize.cx < 1 || clientSize.cy < 1)
    {
      return FALSE;
    }
    *stateOut = NULL;

    state = (MAGVULKANSTATE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*state));
    if (!state || !magGraphicsVulkanLoadGlobalFunctions(state))
    {
      magGraphicsVulkanDestroy(hWnd, state);
      return FALSE;
    }

    applicationInfo.pApplicationName = "mag";
    applicationInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    applicationInfo.pEngineName = "mag";
    applicationInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    applicationInfo.apiVersion = VK_API_VERSION_1_0;
    instanceInfo.pApplicationInfo = &applicationInfo;
    instanceInfo.enabledExtensionCount = ARRAYSIZE(extensions);
    instanceInfo.ppEnabledExtensionNames = extensions;
    if (VK_SUCCESS != state->vkCreateInstance(&instanceInfo, NULL, &state->instance) ||
        !magGraphicsVulkanLoadInstanceFunctions(state))
    {
      magGraphicsVulkanDestroy(hWnd, state);
      return FALSE;
    }

    surfaceInfo.hinstance = GetModuleHandle(NULL);
    surfaceInfo.hwnd = hWnd;
    if (VK_SUCCESS != state->vkCreateWin32SurfaceKHR(
          state->instance,
          &surfaceInfo,
          NULL,
          &state->surface) ||
        !magGraphicsVulkanSelectPhysicalDevice(state) ||
        !magGraphicsVulkanCreateDevice(state) ||
        !magGraphicsVulkanCreateSyncResources(state) ||
        !magGraphicsVulkanCreateSwapchainResources(state, clientSize))
    {
      magGraphicsVulkanDestroy(hWnd, state);
      return FALSE;
    }

    *stateOut = state;
    return TRUE;
}

static void magGraphicsVulkanDestroy(HWND hWnd, void* opaqueState)
{
    MAGVULKANSTATE* state = (MAGVULKANSTATE*)opaqueState;
    UINT i;

    UNREFERENCED_PARAMETER(hWnd);
    if (!state)
    {
      return;
    }

    if (state->device && state->vkDeviceWaitIdle)
    {
      state->vkDeviceWaitIdle(state->device);
    }
    if (state->device && state->vkDestroySwapchainKHR)
    {
      magGraphicsVulkanDestroySwapchainResources(state);
    }
    if (state->device)
    {
      for (i = 0; i < MAG_VK_FRAMES_IN_FLIGHT; ++i)
      {
        if (state->frames[i].fence)
        {
          state->vkDestroyFence(state->device, state->frames[i].fence, NULL);
        }
        if (state->frames[i].renderFinished)
        {
          state->vkDestroySemaphore(state->device, state->frames[i].renderFinished, NULL);
        }
        if (state->frames[i].imageAvailable)
        {
          state->vkDestroySemaphore(state->device, state->frames[i].imageAvailable, NULL);
        }
      }
      if (state->commandPool)
      {
        state->vkDestroyCommandPool(state->device, state->commandPool, NULL);
      }
      if (state->vkDestroyDevice)
      {
        state->vkDestroyDevice(state->device, NULL);
      }
    }
    if (state->surface && state->vkDestroySurfaceKHR)
    {
      state->vkDestroySurfaceKHR(state->instance, state->surface, NULL);
    }
    if (state->instance && state->vkDestroyInstance)
    {
      state->vkDestroyInstance(state->instance, NULL);
    }
    if (state->module)
    {
      FreeLibrary(state->module);
    }
    magGraphicsDestroyCpuCompositor(&state->compositor);
    HeapFree(GetProcessHeap(), 0, state);
}

static BOOL magGraphicsVulkanResize(HWND hWnd, void* opaqueState, SIZE clientSize)
{
    MAGVULKANSTATE* state = (MAGVULKANSTATE*)opaqueState;

    UNREFERENCED_PARAMETER(hWnd);
    if (!state || clientSize.cx < 1 || clientSize.cy < 1)
    {
      return FALSE;
    }
    if ((UINT)clientSize.cx == state->extent.width &&
        (UINT)clientSize.cy == state->extent.height)
    {
      return TRUE;
    }

    if (VK_SUCCESS != state->vkDeviceWaitIdle(state->device))
    {
      return FALSE;
    }
    magGraphicsVulkanDestroySwapchainResources(state);
    return magGraphicsVulkanCreateSwapchainResources(state, clientSize);
}

static BOOL magGraphicsVulkanSetPresentationEnabled(
  HWND hWnd,
  void* opaqueState,
  BOOL enabled)
{
    MAGVULKANSTATE* state = (MAGVULKANSTATE*)opaqueState;
    RECT clientRect;
    SIZE clientSize;

    if (!state || !state->device)
    {
      return FALSE;
    }
    if (!enabled)
    {
      /* Keep the WSI object parked.  Recreating it after a WGL context has
         existed in this process enters vendor ICD teardown/re-entry paths that
         are not stable on all 32-bit drivers.  A parked swap chain owns no
         frame submissions and its last image simply remains latched. */
      return TRUE;
    }
    if (state->swapchain)
    {
      return TRUE;
    }
    if (!GetClientRect(hWnd, &clientRect))
    {
      return FALSE;
    }
    clientSize.cx = max(1L, clientRect.right - clientRect.left);
    clientSize.cy = max(1L, clientRect.bottom - clientRect.top);
    return magGraphicsVulkanCreateSwapchainResources(state, clientSize);
}

static BOOL magGraphicsVulkanRecordCopy(
  MAGVULKANSTATE* state,
  MAGVKFRAME* frame,
  UINT imageIndex)
{
    VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    VkImageMemoryBarrier toTransfer = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    VkImageMemoryBarrier toPresent = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    VkBufferImageCopy copy = { 0 };

    if (VK_SUCCESS != state->vkResetCommandBuffer(frame->commandBuffer, 0) ||
        VK_SUCCESS != state->vkBeginCommandBuffer(frame->commandBuffer, &beginInfo))
    {
      return FALSE;
    }

    toTransfer.srcAccessMask = 0;
    toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toTransfer.oldLayout = state->imageInitialized[imageIndex]
      ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
      : VK_IMAGE_LAYOUT_UNDEFINED;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image = state->images[imageIndex];
    toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toTransfer.subresourceRange.levelCount = 1;
    toTransfer.subresourceRange.layerCount = 1;
    state->vkCmdPipelineBarrier(
      frame->commandBuffer,
      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      0,
      0,
      NULL,
      0,
      NULL,
      1,
      &toTransfer);

    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.layerCount = 1;
    copy.imageExtent.width = state->extent.width;
    copy.imageExtent.height = state->extent.height;
    copy.imageExtent.depth = 1;
    state->vkCmdCopyBufferToImage(
      frame->commandBuffer,
      frame->uploadBuffer,
      state->images[imageIndex],
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      1,
      &copy);

    toPresent.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toPresent.dstAccessMask = 0;
    toPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    toPresent.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toPresent.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toPresent.image = state->images[imageIndex];
    toPresent.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toPresent.subresourceRange.levelCount = 1;
    toPresent.subresourceRange.layerCount = 1;
    state->vkCmdPipelineBarrier(
      frame->commandBuffer,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
      0,
      0,
      NULL,
      0,
      NULL,
      1,
      &toPresent);

    return VK_SUCCESS == state->vkEndCommandBuffer(frame->commandBuffer);
}

static BOOL magGraphicsVulkanRender(
  HWND hWnd,
  void* opaqueState,
  const MAGPIXELBUFFER* sourceFrame,
  const MAGUIDRAWLIST* ui)
{
    MAGVULKANSTATE* state = (MAGVULKANSTATE*)opaqueState;
    MAGVKFRAME* frame;
    MAGPIXELBUFFER composed;
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    UINT imageIndex;
    VkResult result;

    UNREFERENCED_PARAMETER(hWnd);
    if (!state || !sourceFrame ||
        sourceFrame->width != state->extent.width ||
        sourceFrame->height != state->extent.height ||
        !magGraphicsComposeFrame(&state->compositor, sourceFrame, ui, &composed))
    {
      return FALSE;
    }

    frame = &state->frames[state->currentFrame];
    if (VK_SUCCESS != state->vkWaitForFences(
          state->device,
          1,
          &frame->fence,
          VK_TRUE,
          UINT64_MAX))
    {
      return FALSE;
    }

    result = state->vkAcquireNextImageKHR(
      state->device,
      state->swapchain,
      UINT64_MAX,
      frame->imageAvailable,
      VK_NULL_HANDLE,
      &imageIndex);
    if (VK_ERROR_OUT_OF_DATE_KHR == result)
    {
      return FALSE;
    }
    if (VK_SUCCESS != result && VK_SUBOPTIMAL_KHR != result)
    {
      return FALSE;
    }

    if (state->imageFences[imageIndex] &&
        VK_SUCCESS != state->vkWaitForFences(
          state->device,
          1,
          &state->imageFences[imageIndex],
          VK_TRUE,
          UINT64_MAX))
    {
      return FALSE;
    }
    state->imageFences[imageIndex] = frame->fence;

    CopyMemory(
      frame->mappedUpload,
      composed.pixels,
      (SIZE_T)composed.stride * composed.height);
    if (!magGraphicsVulkanRecordCopy(state, frame, imageIndex) ||
        VK_SUCCESS != state->vkResetFences(state->device, 1, &frame->fence))
    {
      return FALSE;
    }

    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &frame->imageAvailable;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frame->commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &frame->renderFinished;
    if (VK_SUCCESS != state->vkQueueSubmit(state->queue, 1, &submitInfo, frame->fence))
    {
      return FALSE;
    }
    state->imageInitialized[imageIndex] = VK_TRUE;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &frame->renderFinished;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &state->swapchain;
    presentInfo.pImageIndices = &imageIndex;
    result = state->vkQueuePresentKHR(state->queue, &presentInfo);
    state->currentFrame = (state->currentFrame + 1U) % MAG_VK_FRAMES_IN_FLIGHT;
    return VK_SUCCESS == result || VK_SUBOPTIMAL_KHR == result;
}

static HANDLE magGraphicsVulkanGetFrameWaitHandle(void* state)
{
    UNREFERENCED_PARAMETER(state);
    return NULL;
}

const MAGGRAPHICSBACKEND g_magGraphicsVulkanBackend =
{
  GRAPHICS_API_VULKAN,
  TEXT("Vulkan"),
  TRUE,
  magGraphicsVulkanIsAvailable,
  magGraphicsVulkanCreate,
  magGraphicsVulkanDestroy,
  magGraphicsVulkanResize,
  magGraphicsVulkanSetPresentationEnabled,
  magGraphicsVulkanRender,
  magGraphicsVulkanGetFrameWaitHandle,
};
