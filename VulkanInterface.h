

// важно подключать после glfw



#include <volk.h>
#include <GLFW/glfw3.h>

#include <shaderc/shaderc.hpp>

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vk_mem_alloc.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <array>
#include <fstream>
#include <iostream>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <tuple>

std::vector<char> ReadBinary(const std::string& file)
{
    std::ifstream stream(file, std::ios::binary);
    if (!stream.is_open())
        throw std::runtime_error("Cannot open file");

    stream.seekg(0, std::ios::end);
    size_t size = stream.tellg();
    stream.seekg(0);

    std::vector<char> data(size);
    stream.read(data.data(), size);

    return data;
}

void PrintDeviceData(VkPhysicalDevice& physicalDevice)
{
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

    // 1. Имя видеокарты
    std::cout << "Name of GPU: " << deviceProperties.deviceName << std::endl;

    // 2. Тип устройства (дискретная, интегрированная и т.д.)
    std::cout << "Type of GPU: ";
    switch (deviceProperties.deviceType) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   std::cout << "Discrete"; break;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: std::cout << "Integrated"; break;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    std::cout << "Virtual"; break;
    case VK_PHYSICAL_DEVICE_TYPE_CPU:            std::cout << "PRocessor"; break;
    default:                                     std::cout << "Unknown"; break;
    }
    std::cout << std::endl;

    // 3. Версия драйвера и API
    std::cout << "API Version Vulkan: "
        << VK_VERSION_MAJOR(deviceProperties.apiVersion) << "."
        << VK_VERSION_MINOR(deviceProperties.apiVersion) << "."
        << VK_VERSION_PATCH(deviceProperties.apiVersion) << std::endl;

}

class InitVulkanInterface;
class VulkanInterfaceCreateDevice;

class VulkanInterface
{
public:
    // TODO: сделать алокатор переменной и инициализировать в init, нужно для правильного уничтожения
    static void Init(InitVulkanInterface& initVulkanInterface, GLFWwindow*& window, const VkAllocationCallbacks* pAllocator = nullptr);
    static void Shutdown(const VkAllocationCallbacks* pAllocator = nullptr)
    {
        vkDestroySurfaceKHR(instance, mainSurface, pAllocator);
        vkDestroyInstance(instance, pAllocator);
    }
    // work with screen
    static VkSurfaceKHR CreateSurface(GLFWwindow* window, const VkAllocationCallbacks* allocator = nullptr);
    static VkSurfaceCapabilitiesKHR CreateSurfaceCapabilities(VkPhysicalDevice& physicalDevice, VkSurfaceKHR& surface);
    static std::vector<VkSurfaceFormatKHR> GetSurfaceFormats(VkPhysicalDevice& physicalDevice, VkSurfaceKHR& surface);
    static std::vector<VkPresentModeKHR> GetPresentModes(VkPhysicalDevice& physicalDevice, VkSurfaceKHR& surface);
    static VkExtent2D CreateExtent2D(VkSurfaceCapabilitiesKHR& surfaceCapabilities, uint32_t defaultWidth = 1280, uint32_t defaultHeight = 720);
    // work with GPU
    static std::vector<VkPhysicalDevice> GetDevices();
    static std::vector<VkQueueFamilyProperties> GetDeviceQueueFamilys(VkPhysicalDevice& physicalDevices);
    static VkQueue CreateQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex = 0);
    static void LoadShader(std::string& path, VkShaderModule& shader, VkDevice& deviceHandle);

    inline static VkInstance instance;
    inline static VkSurfaceKHR mainSurface;
};

class InitVulkanInterface
{
public:

    InitVulkanInterface()
    {
        applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        applicationInfo.pApplicationName = "Vulkan";
        applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        applicationInfo.pEngineName = "Vulkan";
        applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        applicationInfo.apiVersion = VK_API_VERSION_1_3;

        instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceCreateInfo.pApplicationInfo = &applicationInfo;

#ifdef GLFW_VERSION_MAJOR
        uint32_t glfwExtensionCount = 0;
        instanceCreateInfo.ppEnabledExtensionNames = glfwGetRequiredInstanceExtensions(&glfwExtensionCount); // достали расширения для работы с glfw
        instanceCreateInfo.enabledExtensionCount = glfwExtensionCount;
#endif
    }

    void ApplyAppInfo()
    {
        instanceCreateInfo.pApplicationInfo = &applicationInfo;
    }

    VkApplicationInfo applicationInfo{};
    VkInstanceCreateInfo instanceCreateInfo{}; // даем версии и расширения для работы с glfw
};

class VulkanInterfaceCreateDevice
{
public:
    VulkanInterfaceCreateDevice(VkPhysicalDevice& physicalDevice, uint32_t queues, uint32_t queueFamilyIndex)
        : queueCreateInfo(std::vector<VkDeviceQueueCreateInfo>(queues)),
        prioritys(std::vector<std::vector<float>>(queues, std::vector<float>(1, 1.0f))),
        _physicalDevice(physicalDevice),
        _maxQueues(queues),
        _queueFamilyIndex(queueFamilyIndex)
    {
        for (int i = 0; i < queues; i++)
        {
            // должен заполнить queueFamilyIndex
            queueCreateInfo[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo[i].queueCount = 1;
            queueCreateInfo[i].queueFamilyIndex = queueFamilyIndex;
        }

        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.queueCreateInfoCount = queues;
        deviceCreateInfo.pQueueCreateInfos = queueCreateInfo.data();
        deviceCreateInfo.enabledExtensionCount = 0;

        allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    }
    // TODO: Создать удобный заполнитель для queueCreateInfo
    void SetQueues(uint32_t count)
    {
        if (count > _maxQueues)
            throw std::runtime_error("more availible queues is used, use IncreaseQueues");
        deviceCreateInfo.queueCreateInfoCount = count;
    }
    void IncreaseQueues(uint32_t add_count)
    {
        uint32_t old_count = _maxQueues;
        _maxQueues += add_count;
        queueCreateInfo.resize(_maxQueues);
        prioritys.resize(_maxQueues);
        for (uint32_t i = 0; i < add_count; i++)
        {
            queueCreateInfo[old_count + i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo[old_count + i].queueCount = 1;
            queueCreateInfo[i].queueFamilyIndex = _queueFamilyIndex;
        }
    }
    void SetEnabledExtensions(const char* const* extensions, uint32_t count)
    {
        deviceCreateInfo.enabledExtensionCount = count;
        deviceCreateInfo.ppEnabledExtensionNames = extensions;
    }
    void SetPhysicakDevice(VkPhysicalDevice& physicalDevice) { _physicalDevice = physicalDevice; }
    void ApplyPrioritys()
    {
        for (int i = 0; i < deviceCreateInfo.queueCreateInfoCount; i++)
            queueCreateInfo[i].pQueuePriorities = prioritys[i].data();
    }
    VkDevice CreateDevice(const VkAllocationCallbacks* pAllocator = nullptr)
    {
        VkDevice deviceHandle;
        if (vkCreateDevice(_physicalDevice, &deviceCreateInfo, pAllocator, &deviceHandle) != VK_SUCCESS)
            throw std::runtime_error("vkCreateDevice");
        volkLoadDevice(deviceHandle); // загрузили прямые и самые быстрые укащатели для конкретно этой карты

        vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

        allocatorCreateInfo.instance = VulkanInterface::instance;
        allocatorCreateInfo.physicalDevice = _physicalDevice;
        allocatorCreateInfo.device = deviceHandle;
        allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions; // Связываем volk и VMA
        allocatorCreateInfo.pAllocationCallbacks = pAllocator;

        return deviceHandle;
    }
    VmaAllocator CreateAllocator()
    {
        VmaAllocator allocator;
        if (vmaCreateAllocator(&allocatorCreateInfo, &allocator) != VK_SUCCESS)
            throw std::runtime_error("Failed to create VMA allocator");

        return allocator;
    }

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfo;
    std::vector<std::vector<float>> prioritys;
    VkDeviceCreateInfo deviceCreateInfo{};

    VmaVulkanFunctions vulkanFunctions{}; // передаем функции для работы
    VmaAllocatorCreateInfo allocatorCreateInfo{};

private:
    uint32_t _queueFamilyIndex;
    uint32_t _maxQueues;
    VkPhysicalDevice& _physicalDevice;
};

class VulkanInterfaceCreateSwapchain
{
public:
    VulkanInterfaceCreateSwapchain(VkSurfaceCapabilitiesKHR& surfaceCapabilities, VkSurfaceKHR& surface, VkSurfaceFormatKHR surfaceFormat)
    {
        uint32_t imageCount = surfaceCapabilities.minImageCount + 1; // выставляем количество буфферов + 1 для кайфа
        if (surfaceCapabilities.maxImageCount && imageCount > surfaceCapabilities.maxImageCount) // если перебрали, то уменьшаем
            imageCount = surfaceCapabilities.maxImageCount;

        swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchainCreateInfo.surface = surface;
        swapchainCreateInfo.minImageCount = imageCount;
        swapchainCreateInfo.imageFormat = surfaceFormat.format;
        swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
        swapchainCreateInfo.imageExtent = surfaceCapabilities.currentExtent;
        swapchainCreateInfo.imageArrayLayers = 1; // количество картинок на кадр (типо для VR надо 2)
        swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // в картинки идет рендер цвета
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // картинками владеет только графическая очередь
        swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform; // не переворачивать картинку (актуально для смартфонов)
        swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // Окно игры непрозрачное (рабочий стол сквозь него не виден)
        swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR; // пиксели, скрытые другими окнами видеокарта не будет обсчитывать
        swapchainCreateInfo.clipped = VK_TRUE;

        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; // просто 2D текстура
        imageViewCreateInfo.format = surfaceFormat.format;
        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // работаем с каналами цвета
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.levelCount = 1;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = 1;
    }
    
    VkSwapchainKHR CreateSwapchain(VkDevice& deviceHandle)
    {
        VkSwapchainKHR swapchain;
        if (vkCreateSwapchainKHR(deviceHandle, &swapchainCreateInfo, nullptr, &swapchain) != VK_SUCCESS)
            throw std::runtime_error("vkCreateSwapchainKHR");
        return swapchain;
    }
    void FillViews(std::vector<VkImage>& swapchainImages, std::vector<VkImageView>& swapchainImageViews, 
        VkSwapchainKHR& swapchain, VkDevice& deviceHandle, const VkAllocationCallbacks* pAllocator = nullptr)
    {
        uint32_t swapchainImageCount = 0; // забираем картинки сделанные для swapchain
        vkGetSwapchainImagesKHR(deviceHandle, swapchain, &swapchainImageCount, nullptr);
        swapchainImages.resize(swapchainImageCount);
        vkGetSwapchainImagesKHR(deviceHandle, swapchain, &swapchainImageCount, swapchainImages.data());

        swapchainImageViews.resize(swapchainImageCount); // создаем возможность правильно понимать данные текстуры
        for (uint32_t i = 0; i < swapchainImageCount; i++)
        {
            imageViewCreateInfo.image = swapchainImages[i];
            if (vkCreateImageView(deviceHandle, &imageViewCreateInfo, pAllocator, &swapchainImageViews[i]) != VK_SUCCESS)
                throw std::runtime_error("vkCreateImageView");
        }
    }
    VkImageViewCreateInfo imageViewCreateInfo{};
    VkSwapchainCreateInfoKHR swapchainCreateInfo{};
};

class VulkanInterfaceCreateCommandPool
{
public:
    VulkanInterfaceCreateCommandPool(uint32_t queueFamilyIndex, uint32_t swapchainImageCount)
    {
        commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // выделить много памяти, каждый кадр перезаписывать
        commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;
        
        commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandBufferAllocateInfo.commandBufferCount = swapchainImageCount;
        _swapchainImageCount = swapchainImageCount;
    }
    void SetSwapchainImageCount(uint32_t swapchainImageCount)
    {
        commandBufferAllocateInfo.commandBufferCount = swapchainImageCount;
        _swapchainImageCount = swapchainImageCount;
    }
    VkCommandPool CreateCommandPool(VkDevice& device, const VkAllocationCallbacks* pAllocator = nullptr)
    {
        VkCommandPool commandPool;
        if (vkCreateCommandPool(device, &commandPoolCreateInfo, pAllocator, &commandPool) != VK_SUCCESS)
            throw std::runtime_error("vkCreateCommandPool");
        commandBufferAllocateInfo.commandPool = commandPool;
        
        return commandPool;
    }
    std::vector<VkCommandBuffer> CreateCommandBuffers(VkDevice& device, const VkAllocationCallbacks* pAllocator = nullptr)
    {
        std::vector<VkCommandBuffer> commandBuffers(_swapchainImageCount); // создание буфера команд под каждую картинку
        if (vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, commandBuffers.data()) != VK_SUCCESS)
            throw std::runtime_error("vkAllocateCommandBuffers");
        return commandBuffers;
    }


    VkCommandPoolCreateInfo commandPoolCreateInfo{}; // пул для GPU команд 
    VkCommandBufferAllocateInfo commandBufferAllocateInfo{}; 
private:
    uint32_t _swapchainImageCount;
};

class VulkanInterfaceCreateSemaphore
{
public:
    VulkanInterfaceCreateSemaphore()
    {
        semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    }

    VkSemaphore Create(VkDevice& deviceHandle, const VkAllocationCallbacks* pAllocator = nullptr)
    {
        VkSemaphore semaphore; // рендеринг закончен
        vkCreateSemaphore(deviceHandle, &semaphoreCreateInfo, pAllocator, &semaphore);
        return semaphore;
    }

    VkSemaphoreCreateInfo semaphoreCreateInfo{};
};

class VulkanInterfaceCreateFence
{
public:
    VulkanInterfaceCreateFence()
    {
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    }

    VkFence Create(VkDevice& deviceHandle, const VkAllocationCallbacks* pAllocator = nullptr)
    {

        VkFence fence;
        vkCreateFence(deviceHandle, &fenceCreateInfo, pAllocator, &fence);
        return fence;
    }

    VkFenceCreateInfo fenceCreateInfo{};
};

class VulkanInterfaceCreateImage 
{
public:
    VulkanInterfaceCreateImage(uint32_t width, uint32_t height, VkFormat format, 
        VkImageUsageFlags usage = 
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
    {
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.extent.width = width;
        imageCreateInfo.extent.height = height;
        imageCreateInfo.extent.depth = 1;
        imageCreateInfo.mipLevels = 1;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.format = format;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL; // говорим по крутому разместить в памяти, CPU читать не сможет, но GPU будет летать
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // плевать что лежало до нас
        imageCreateInfo.usage = usage;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        // Объявляем аллокацию вместо VkDeviceMemory
        allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO; // подбери память сам
        allocationCreateInfo.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT; // но хотелось бы DEVICE_LOCAL_BIT

        // Этот вызов сам создаст картинку и выделит под нее оптимальную память
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = format;
        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.levelCount = 1;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = 1;
    }
    // TODO: написать вспомогательные функции
    std::tuple<VkImage, VkImageView, VmaAllocation> Create(VkDevice& device, VmaAllocator& allocator, 
        VmaAllocationInfo* pAllocationInfo = nullptr, const VkAllocationCallbacks* pAllocator = nullptr)
    {
        VmaAllocation allocation;
        VkImage image;
        if (vmaCreateImage(allocator, &imageCreateInfo, &allocationCreateInfo,
            &image, &allocation, pAllocationInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create offscreen image via VMA");
        }
        imageViewCreateInfo.image = image;
        VkImageView imageView;
        vkCreateImageView(device, &imageViewCreateInfo, pAllocator, &imageView);
        return std::tuple<VkImage, VkImageView, VmaAllocation>(image, imageView, allocation);
    }    

    VkImageCreateInfo imageCreateInfo{};
    VkImageViewCreateInfo imageViewCreateInfo{};
    VmaAllocationCreateInfo allocationCreateInfo{};
private:

};

class VulkanInterfaceCreateSampler
{
public:
    VulkanInterfaceCreateSampler()
    {
         // правила фильтрации текстуры
        samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCreateInfo.maxLod = 1.0f;
    }
    VkSampler Create(VkDevice& device, const VkAllocationCallbacks* pAllocator = nullptr)
    {
        VkSampler sampler;
        vkCreateSampler(device, &samplerCreateInfo, pAllocator, &sampler);
        return sampler;
    }
    VkSamplerCreateInfo samplerCreateInfo{};
};

void VulkanInterface::Init(InitVulkanInterface& initVulkanInterface, GLFWwindow*& window, const VkAllocationCallbacks* pAllocator)
    {
        volkInitialize();

        if (vkCreateInstance(&initVulkanInterface.instanceCreateInfo, pAllocator, &instance) != VK_SUCCESS)
            throw std::runtime_error("vkCreateInstance");
        volkLoadInstance(instance);

        mainSurface = CreateSurface(window);
    }

// work with screen
VkSurfaceKHR VulkanInterface::CreateSurface(GLFWwindow* window, const VkAllocationCallbacks* allocator)
    {
        // поверхность конкретно нашего окна
        VkSurfaceKHR surface;
#ifdef GLFW_VERSION_MAJOR
        if (glfwCreateWindowSurface(instance, window, allocator, &surface) != VK_SUCCESS)
            throw std::runtime_error("CreateSurface");
#else
        throw std::logic_error("API is support only GLFW");
#endif
        return surface;
    }

inline VkSurfaceCapabilitiesKHR VulkanInterface::CreateSurfaceCapabilities(VkPhysicalDevice& physicalDevice, VkSurfaceKHR& surface)
{
    VkSurfaceCapabilitiesKHR surfaceCapabilities; // возможности работы с монитором (min и max разрешение и поддерживоемое кол-во буфферов)
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);
    return surfaceCapabilities;
}

inline std::vector<VkSurfaceFormatKHR> VulkanInterface::GetSurfaceFormats(VkPhysicalDevice& physicalDevice, VkSurfaceKHR& surface)
{

    uint32_t surfaceFormatCount = 0; // поддерживаемые цветовые форматы
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, surfaceFormats.data());
    return surfaceFormats;
    //VkSurfaceFormatKHR surfaceFormat = surfaceFormats[0]; // ? должен быть цикл с выбором лучшего
    //for (const auto& format : surfaceFormats)
    //{
    //    if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
    //        format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
    //    {
    //        surfaceFormat = format;
    //        break;
    //    }
    //}
}

std::vector<VkPresentModeKHR> VulkanInterface::GetPresentModes(VkPhysicalDevice& physicalDevice, VkSurfaceKHR& surface)
{
    uint32_t presentModeCount = 0; // как менять кадры
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());
    return presentModes;
    //VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR; // базовый v-sync
    //for (auto mode : presentModes)
    //    if (mode == VK_PRESENT_MODE_MAILBOX_KHR) // тройная буферизация и Fast Sync
    //    {
    //        presentMode = mode;
    //        break;
    //    }
}

VkExtent2D VulkanInterface::CreateExtent2D(VkSurfaceCapabilitiesKHR& surfaceCapabilities, uint32_t defaultWidth, uint32_t defaultHeight)
{
    VkExtent2D extent;
    if (surfaceCapabilities.currentExtent.width != UINT32_MAX) // система дала нам конкретный размер
        extent = surfaceCapabilities.currentExtent;
    else // не дала и мы делаем че хотим
    {
        extent.width = defaultWidth;
        extent.height = defaultHeight;
    }
    return extent;
}

// work with GPU
std::vector<VkPhysicalDevice> VulkanInterface::GetDevices()
    {
        uint32_t physicalDeviceCount;
        vkEnumeratePhysicalDevices(VulkanInterface::instance, &physicalDeviceCount, nullptr);
        std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
        vkEnumeratePhysicalDevices(VulkanInterface::instance, &physicalDeviceCount, physicalDevices.data());
        return physicalDevices;
    }

std::vector<VkQueueFamilyProperties> VulkanInterface::GetDeviceQueueFamilys(VkPhysicalDevice& physicalDevices)
    {
        uint32_t familyCount = 0;  // получили все семейства чипов на карте
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices, &familyCount, nullptr);
        std::vector<VkQueueFamilyProperties> families(familyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices, &familyCount, families.data());
        return families;
    }

VkQueue VulkanInterface::CreateQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex)
{
    VkQueue queue; // наша графическая очередь (будем отправлять команды через нее)
    vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, &queue);
    return queue;
}

void VulkanInterface::LoadShader(std::string &path, VkShaderModule& shader, VkDevice &deviceHandle)
    {
        auto vertexShaderCode = ReadBinary(path);

        static VkShaderModuleCreateInfo shaderModuleCreateInfo{};
        shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderModuleCreateInfo.codeSize = vertexShaderCode.size();
        shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(vertexShaderCode.data());

        vkCreateShaderModule(deviceHandle, &shaderModuleCreateInfo, nullptr, &shader);
    }

