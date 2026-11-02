

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
    static VkShaderModule LoadShader(std::string path, VkDevice& device, const VkAllocationCallbacks* pAllocator = nullptr);

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

        vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

        allocatorCreateInfo.instance = VulkanInterface::instance;
        allocatorCreateInfo.physicalDevice = _physicalDevice;
        allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions; // Связываем volk и VMA
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
    std::tuple<VkDevice, VmaAllocator> Create(const VkAllocationCallbacks* pAllocator = nullptr)
    {
        VkDevice deviceHandle;
        if (vkCreateDevice(_physicalDevice, &deviceCreateInfo, pAllocator, &deviceHandle) != VK_SUCCESS)
            throw std::runtime_error("vkCreateDevice");
        volkLoadDevice(deviceHandle); // загрузили прямые и самые быстрые укащатели для конкретно этой карты

        allocatorCreateInfo.device = deviceHandle;
        //allocatorCreateInfo.pAllocationCallbacks = pAllocator;

        VmaAllocator allocator;
        if (vmaCreateAllocator(&allocatorCreateInfo, &allocator) != VK_SUCCESS)
            throw std::runtime_error("Failed to create VMA allocator");

        return std::tuple<VkDevice, VmaAllocator>(deviceHandle, allocator);
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

class VulkanInterfaceCreateBuffer
{
public:
    VulkanInterfaceCreateBuffer(VkDeviceSize byteSize, VkBufferUsageFlags usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VmaAllocationCreateFlags allocFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT)
    //                                        пишем последовательно и CPU не может читать              открыть память для прямой записи
    {
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.size = byteSize;
        bufferCreateInfo.usage = usage;
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocationInfo.flags = allocFlags;
    }

    std::tuple<VkBuffer, VmaAllocation> Create(const void* data, size_t dataSize, VmaAllocator& allocator)
    {
        VmaAllocationInfo pAllocationInfo{};
        VkBuffer vertexBuffer;
        VmaAllocation vertexAllocation; // просто handl выделенной памяти
        if (vmaCreateBuffer(allocator, &bufferCreateInfo, &allocationInfo,
            &vertexBuffer, &vertexAllocation, &pAllocationInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create vertex buffer via VMA");
        }

        if (pAllocationInfo.pMappedData != nullptr)
        {
            VmaAllocationInfo vertexAllocData; // конкретные данные выделенной памяти
            vmaGetAllocationInfo(allocator, vertexAllocation, &vertexAllocData);
            memcpy(vertexAllocData.pMappedData, data, dataSize);
        }
        else
        {
            void* pData;
            vmaMapMemory(allocator, vertexAllocation, &pData);
            memcpy(pData, data, dataSize);
            vmaUnmapMemory(allocator, vertexAllocation);
        }
        return std::tuple<VkBuffer, VmaAllocation>(vertexBuffer, vertexAllocation);
    }
    std::tuple<VkBuffer, VmaAllocation> Create(const void* data, size_t dataSize, VmaAllocator& allocator,
        VmaAllocationInfo* pAllocationInfo)
    {
        VkBuffer vertexBuffer;
        VmaAllocation vertexAllocation; // просто handl выделенной памяти
        if (vmaCreateBuffer(allocator, &bufferCreateInfo, &allocationInfo,
            &vertexBuffer, &vertexAllocation, pAllocationInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create vertex buffer via VMA");
        }
        assert(pAllocationInfo != nullptr);
        if (pAllocationInfo->pMappedData != nullptr)
        {
            VmaAllocationInfo vertexAllocData; // конкретные данные выделенной памяти
            vmaGetAllocationInfo(allocator, vertexAllocation, &vertexAllocData);
            memcpy(vertexAllocData.pMappedData, data, dataSize);
        }
        else
        {
            void* pData;
            vmaMapMemory(allocator, vertexAllocation, &pData);
            memcpy(pData, data, dataSize);
            vmaUnmapMemory(allocator, vertexAllocation);
        }
        return std::tuple<VkBuffer, VmaAllocation>(vertexBuffer, vertexAllocation);
    }

    inline void NewBuffer(VkDeviceSize byteSize, VkBufferUsageFlags usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT)
    {
        bufferCreateInfo.usage = usage;
        bufferCreateInfo.size = byteSize;
    }

    VkBufferCreateInfo bufferCreateInfo{};
    VmaAllocationCreateInfo allocationInfo{};
};

// Говорит как группировать биндинги (сгруппированные биндинги - сеты)
class VulkanInterfaceCreateDescriptorSetLayout
{
public:
    VulkanInterfaceCreateDescriptorSetLayout(uint32_t bindingCount = 1)
        : bindings(1, {})
    {
        descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutCreateInfo.bindingCount = bindingCount;
        descriptorSetLayoutCreateInfo.pBindings = bindings.data();
    }

    template <typename ...Args>
    inline void FillBindings(uint32_t binding = 0, uint32_t descriptorCount = 1, 
        VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
        VkShaderStageFlags stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        Args... args)
    {
        _FillBindings(0, binding, descriptorCount, descriptorType, stageFlags, args...);
    }

    VkDescriptorSetLayout Create(VkDevice& device, const VkAllocationCallbacks* pAllocator = nullptr)
    {
        VkDescriptorSetLayout descriptorSetLayout; // описывает один set
        vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, pAllocator, &descriptorSetLayout);
        return descriptorSetLayout;
    }

    std::vector<VkDescriptorSetLayoutBinding> bindings;
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};

private:
     
    template <typename ...Args>
    void _FillBindings(uint32_t Idx, uint32_t binding, uint32_t descriptorCount,
        VkDescriptorType descriptorType, VkShaderStageFlags stageFlags,
        Args... args)
    {
        bindings[Idx].binding = binding;
        bindings[Idx].descriptorCount = descriptorCount;
        bindings[Idx].descriptorType = descriptorType;
        bindings[Idx].stageFlags = stageFlags;
        if constexpr (sizeof...(args) > 0)
        {
            FillBindings(Idx + 1, args...);
        }
    }
};
// TODO: Найти хуйню где два pAllocator типо от него зависит какой-то там механизм выделения автоматической или ручной памяти 
// Говорит сколько и какого типа выделять эти самые биндинги и сколько максимум сетов
class VulkanInterfaceCreateDescriptorPool
{
public:
    VulkanInterfaceCreateDescriptorPool(uint32_t maxSets=1, uint32_t descriptorPoolSizeCount = 1)
        : descriptorPoolSizes(descriptorPoolSizeCount, {})
    {
        descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptorPoolCreateInfo.maxSets = maxSets;
        descriptorPoolCreateInfo.poolSizeCount = descriptorPoolSizes.size();
        descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes.data();
    }

    template<typename... DescriptiorPoolSizes>
    void FillDescriptionPoolSize(DescriptiorPoolSizes... PoolSizes)
    {
        if constexpr (sizeof...(PoolSizes) > 0)
            _FillDescriptionPoolSize(0, PoolSizes...);
    }

    template<typename... DescriptiorPoolSizes>
    void FillDescriptionPoolSize(uint32_t startIdx, DescriptiorPoolSizes... PoolSizes)
    {
        if constexpr (sizeof...(PoolSizes) > 0)
            _FillDescriptionPoolSize(startIdx, PoolSizes...);
    }

    VkDescriptorPool Create(VkDevice& device, const VkAllocationCallbacks* pAllocator = nullptr)
    {
        VkDescriptorPool descriptorPool;
        vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, pAllocator, &descriptorPool);
        return descriptorPool;
    }

    std::vector<VkDescriptorPoolSize> descriptorPoolSizes;
    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
private:
    template<typename... DescriptiorPoolSizes>
    void _FillDescriptionPoolSize(uint32_t idx, VkDescriptorType dType, uint32_t dCount, 
        DescriptiorPoolSizes... PoolSizes)
    {
        descriptorPoolSizes[idx].type = dType;
        descriptorPoolSizes[idx].descriptorCount = dCount;
        if constexpr (sizeof...(PoolSizes) > 0)
        {
            _FillDescriptionPoolSize(idx+1, PoolSizes...);
        }
    }
};

class VulkanInterfaceCreateDescriptorSet
{
public:
    VulkanInterfaceCreateDescriptorSet(VkDescriptorPool& pool, VkDescriptorSetLayout* setLayouts, uint32_t layoutCount=1)
    {
        descriptorAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptorAllocateInfo.descriptorPool = pool;
        descriptorAllocateInfo.descriptorSetCount = layoutCount;
        descriptorAllocateInfo.pSetLayouts = setLayouts;
    }

    std::vector<VkDescriptorSet> Create(VkDevice& device)
    {
        std::vector<VkDescriptorSet> descriptorSet(descriptorAllocateInfo.descriptorSetCount); // объединяет контейнер и все описания layout'ов
        vkAllocateDescriptorSets(device, &descriptorAllocateInfo, descriptorSet.data());
        return descriptorSet;
    }

    // Only if descriptorSetCount == 1
    VkDescriptorSet CreateOne(VkDevice& device)
    {
        assert(descriptorAllocateInfo.descriptorSetCount == 1);
        VkDescriptorSet descriptorSet; // объединяет контейнер и все описания layout'ов
        vkAllocateDescriptorSets(device, &descriptorAllocateInfo, &descriptorSet);
        return descriptorSet;
    }
    
    VkDescriptorSetAllocateInfo descriptorAllocateInfo{};
};
// TODO: Спросить про шаблоны и лучше давать аргументы в первой функции или только в приватной? И вообще спросить про аргументы в этой хуйне
class VulkanInterfaceWriteDescriptorSet
{ 
public:
    VulkanInterfaceWriteDescriptorSet(uint32_t imageInfoCount, uint32_t bufferInfoCount, uint32_t texelBufferViewCount,
        uint32_t writeDescriptorCount, uint32_t copyDescriptorCount = 0)
        : imageInfo(imageInfoCount, {}), bufferInfo(bufferInfoCount, {}), texelBufferView(texelBufferViewCount, {}),
        writeDescriptorSets(writeDescriptorCount, {}), copyDescriptorSets(copyDescriptorCount, {})
    {
    }

    void FillDescriptorImageInfo(uint32_t idx, VkImageLayout layout, VkImageView view, VkSampler sampler)
    {
        imageInfo[idx].imageLayout = layout;
        imageInfo[idx].imageView = view;
        imageInfo[idx].sampler = sampler;
    }

    void FillDescriptorBufferInfo(uint32_t idx, VkBuffer& buffer, VkDeviceSize offset, VkDeviceSize range)
    {
        bufferInfo[idx].buffer = buffer;
        bufferInfo[idx].offset = offset;
        bufferInfo[idx].range = range;
    }

    void FillDescriptorSet(uint32_t idx, VkDescriptorSet& set, VkDescriptorType dType,
        uint32_t binding = 0, uint32_t startArrayIdx = 0, uint32_t descriptorCount = 1,
        uint32_t imageInfoOffset = 0, uint32_t bufferInfooffset = 0, uint32_t texelBufferViewOffset = 0)
    {
        writeDescriptorSets[idx].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSets[idx].dstSet = set;                 // 1. Куда пишем? В нашу пустую коробочку-сет
        writeDescriptorSets[idx].dstBinding = binding;         // 2. В какую розетку пишем? В биндинг №0
        writeDescriptorSets[idx].dstArrayElement = startArrayIdx;          // 3. С какого элемента массива начать? С нулевого
        writeDescriptorSets[idx].descriptorCount = descriptorCount;          // 4. Сколько розеток заполняем за раз? Одну
        writeDescriptorSets[idx].descriptorType = dType;       // 5. Тип того, что вставляем
        writeDescriptorSets[idx].pImageInfo = imageInfo.data() + imageInfoOffset; // 6. Ссылка на саму текстуру и её сглаживатель
        writeDescriptorSets[idx].pBufferInfo = bufferInfo.data() + bufferInfooffset;
        writeDescriptorSets[idx].pTexelBufferView = texelBufferView.data() + texelBufferViewOffset;
    }

    void FillCopyDescriptorSet(uint32_t idx,
        VkDescriptorSet& srcSet, uint32_t srcBinding, uint32_t srcArrayIdx,          // если копируем массив то выбираем сколько элементов
        VkDescriptorSet& dstSet, uint32_t dstBinding, uint32_t dstArrayIdx, uint32_t descriptorCount)
    {
        copyDescriptorSets[idx].sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET;
        copyDescriptorSets[idx].srcSet = srcSet;
        copyDescriptorSets[idx].srcBinding = srcBinding;
        copyDescriptorSets[idx].srcArrayElement = srcArrayIdx;
        copyDescriptorSets[idx].dstSet = dstSet;
        copyDescriptorSets[idx].dstBinding = dstBinding;
        copyDescriptorSets[idx].dstArrayElement = dstArrayIdx;
        copyDescriptorSets[idx].descriptorCount = descriptorCount;
    }

    inline void Update(VkDevice& device)
    {
        vkUpdateDescriptorSets(device, 
            writeDescriptorSets.size(), writeDescriptorSets.data(), 
            copyDescriptorSets.size(), copyDescriptorSets.data()
        );
    }
    inline void Update(VkDevice& device, 
        uint32_t sizeWrite, uint32_t sizeCopy)
    {
        vkUpdateDescriptorSets(device, 
            sizeWrite, writeDescriptorSets.data(),
            sizeCopy, copyDescriptorSets.data()
        );
    }
    inline void Update(VkDevice& device, 
        uint32_t sizeWrite, uint32_t sizeCopy, 
        uint32_t offsetWrite, uint32_t offsetCopy)
    {
        vkUpdateDescriptorSets(device, 
            sizeWrite, writeDescriptorSets.data() + offsetWrite,
            sizeCopy, copyDescriptorSets.data() + offsetCopy
        );
    }

    struct WriteDescriptorSet : public VkWriteDescriptorSet
    {
        WriteDescriptorSet() : VkWriteDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET } {}
    };
    struct CopyDescriptorSet : public VkCopyDescriptorSet
    {
        CopyDescriptorSet() : VkCopyDescriptorSet{ VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET } {}
    };
    static_assert(sizeof(WriteDescriptorSet) == sizeof(VkWriteDescriptorSet));
    static_assert(sizeof(CopyDescriptorSet) == sizeof(VkCopyDescriptorSet));

    std::vector<WriteDescriptorSet> writeDescriptorSets;
    std::vector<CopyDescriptorSet> copyDescriptorSets;
    std::vector<VkDescriptorImageInfo> imageInfo;
    std::vector<VkDescriptorBufferInfo> bufferInfo;
    std::vector<VkBufferView> texelBufferView;
};
















// TODO: Сейчас это больште векторы с разным использованием
        // Хочу чтобы векторы создавались под конкретный Pipeline и можно было спокойно брать data() и size() ||| и еще функции что бы скинуть отдельные части для последующих Pipeline


class VulkanInterfaceCreatePipeline
{
public:
    VulkanInterfaceCreatePipeline(VulkanInterfaceCreatePipeline&) = delete;
    VulkanInterfaceCreatePipeline(VulkanInterfaceCreatePipeline&&) = delete;

    VulkanInterfaceCreatePipeline(uint32_t shaderStagesCount, uint32_t bindingsCount, uint32_t attributesCount, 
        uint32_t viewportsCount, uint32_t scissorsCount, uint32_t blendAttachmentsCount, uint32_t dynamicStatesCount)
        : shaderStages(shaderStagesCount, {}), bindings(bindingsCount, {}), attributes(attributesCount, {}), 
        viewports(viewportsCount, {}), scissors(scissorsCount, {}), 
        blendAttachments(blendAttachmentsCount, {}), dynamicStates(dynamicStatesCount, {})
    {

        pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

        vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // в какую геометрию собираем

        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_NONE; // когда показывать грань
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // когда считать что грань повернута лицом

        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT; // отрисовывать честно пиксель
        
        colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendState.logicOpEnable = VK_FALSE; // логические попиксельные операции

        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;

        renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    }

    inline void SetPushConstants(VkPushConstantRange* pushConstantRanges, uint32_t count = 1)
    {
        pipelineLayoutCreateInfo.pushConstantRangeCount = count;
        pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges;
    }
    inline void SetDescriptorSetLayouts(VkDescriptorSetLayout* descriptorSetLayouts, uint32_t count = 1)
    {
        pipelineLayoutCreateInfo.setLayoutCount = count;
        pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts;
    }

    void FillShaderStage(uint32_t idx, VkShaderModule shaderModule, VkShaderStageFlagBits stageFlag = VK_SHADER_STAGE_VERTEX_BIT, const char* startFuncName = "main")
    {
        shaderStages[idx].stage = stageFlag;
        shaderStages[idx].module = shaderModule;
        shaderStages[idx].pName = startFuncName;
    }

    void FillBinding(uint32_t idx, uint32_t binding, uint32_t stride, VkVertexInputRate inputRate)
    {
        bindings[idx].binding = binding; // номер аппаратного слота
        bindings[idx].stride = stride; // размер одной вершины
        bindings[idx].inputRate = inputRate;
    }

    void FillAttribute(uint32_t idx, uint32_t binding, uint32_t location, uint32_t offset, VkFormat format)
    {
        attributes[idx].binding = binding;
        attributes[idx].location = location;
        attributes[idx].format = format;
        attributes[idx].offset = offset;
    }
    
    inline void FillViewport(uint32_t idx, VkExtent2D extent, float x = 0, float y = 0, float minDepth = 0.0, float maxDepth = 1.0)
    {
        viewports[idx].x = x;
        viewports[idx].y = y;
        viewports[idx].width = static_cast<float>(extent.width);
        viewports[idx].height = static_cast<float>(extent.height);
        viewports[idx].minDepth = minDepth;
        viewports[idx].maxDepth = maxDepth;
    }
    inline void FillViewport(uint32_t idx, float width, float height, float x = 0, float y = 0, float minDepth = 0.0, float maxDepth = 1.0)
    {
        viewports[idx].x = x;
        viewports[idx].y = y;
        viewports[idx].width = width;
        viewports[idx].height = height;
        viewports[idx].minDepth = minDepth;
        viewports[idx].maxDepth = maxDepth;
    }
    inline void FillScissor(uint32_t idx, VkExtent2D extent, VkOffset2D offset = {0, 0})
    {
        scissors[idx].extent = extent;
        scissors[idx].offset = offset;
    }
    
    // TODO: Чет тут с аргументами по умолчанию точно не то
    inline void FillBlendAttachment(uint32_t idx,
        VkBool32              blendEnable = VK_FALSE,
        VkBlendFactor         srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        VkBlendFactor         dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        VkBlendOp             colorBlendOp = VK_BLEND_OP_ADD,
        VkBlendFactor         srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        VkBlendFactor         dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        VkBlendOp             alphaBlendOp = VK_BLEND_OP_ADD,
        VkColorComponentFlags colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT)
    {
        blendAttachments[idx].blendEnable = blendEnable;
        blendAttachments[idx].srcColorBlendFactor = srcColorBlendFactor;
        blendAttachments[idx].dstColorBlendFactor = dstColorBlendFactor;
        blendAttachments[idx].colorBlendOp = colorBlendOp;
        blendAttachments[idx].srcAlphaBlendFactor = srcAlphaBlendFactor;
        blendAttachments[idx].dstAlphaBlendFactor = dstAlphaBlendFactor;
        blendAttachments[idx].alphaBlendOp = alphaBlendOp;
        blendAttachments[idx].colorWriteMask = colorWriteMask;
    }

    inline void SetRenderingInfoFormats(VkFormat* formats, uint32_t count)
    {
        renderingInfo.colorAttachmentCount = count;
        renderingInfo.pColorAttachmentFormats = formats; // данные о цветовой политре surface
    }

    inline void FillRenderingInfo(VkFormat* formats, uint32_t count, uint32_t viewMask, 
        VkFormat depthAttachmentFormat, VkFormat stencilAttachmentFormat)
    {
        renderingInfo.viewMask = viewMask;
        renderingInfo.colorAttachmentCount = count;
        renderingInfo.pColorAttachmentFormats = formats; // данные о цветовой политре surface
        renderingInfo.depthAttachmentFormat = depthAttachmentFormat;
        renderingInfo.stencilAttachmentFormat = stencilAttachmentFormat;
    }

    std::tuple<VkPipelineLayout, VkPipeline> Create(VkDevice& device, const VkAllocationCallbacks* pAllocatorPipelineLayout = nullptr,
        const VkAllocationCallbacks* pAllocatorPipeline = nullptr, VkPipelineCache pipelineCache = VK_NULL_HANDLE)
    {
        VkPipelineLayout pipelineLayout;                                                                                                  // VkPipelineLayout (ресурсы для стадии \ дескрипторы, пуши и тд)
        vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, pAllocatorPipelineLayout, &pipelineLayout);

        viewportState.viewportCount = viewports.size();
        viewportState.pViewports = viewports.data();
        viewportState.scissorCount = scissors.size();
        viewportState.pScissors = scissors.data();

        vertexInputState.vertexBindingDescriptionCount = bindings.size();
        vertexInputState.pVertexBindingDescriptions = bindings.data();
        vertexInputState.vertexAttributeDescriptionCount = attributes.size();
        vertexInputState.pVertexAttributeDescriptions = attributes.data();

        colorBlendState.attachmentCount = blendAttachments.size(); // настройка цвета только для одной текстуры
        colorBlendState.pAttachments = blendAttachments.data();

        dynamicState.dynamicStateCount = dynamicStates.size();
        dynamicState.pDynamicStates = dynamicStates.data();

        // есть еще VkComputePipelineCreateInfo
        VkGraphicsPipelineCreateInfo pipelineCreateInfo{};                                                                                      // VkGraphicsPipelineCreateInfo (собирает все в один pipeline \ можно указать от какого pipeline отталкиваться при компиляции)
        pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO; // sType сдесь разный, может быть VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
        pipelineCreateInfo.pNext = &renderingInfo;
        pipelineCreateInfo.stageCount = shaderStages.size(); // бедет задействованы два шейдера
        pipelineCreateInfo.pStages = shaderStages.data(); // шейдеры
        pipelineCreateInfo.pVertexInputState = &vertexInputState;
        pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
        pipelineCreateInfo.pViewportState = &viewportState;
        pipelineCreateInfo.pRasterizationState = &rasterizer;
        pipelineCreateInfo.pMultisampleState = &multisampling;
        pipelineCreateInfo.pColorBlendState = &colorBlendState;
        pipelineCreateInfo.pDynamicState = &dynamicState;
        pipelineCreateInfo.layout = pipelineLayout;
        VkPipeline scenePipeline;
        if (vkCreateGraphicsPipelines(
            device,
            pipelineCache,
            1,
            &pipelineCreateInfo,
            pAllocatorPipeline,
            &scenePipeline) != VK_SUCCESS)
        {
            throw std::runtime_error("Scene pipeline not create");
        }
        return std::tuple<VkPipelineLayout, VkPipeline>(pipelineLayout, scenePipeline);
    }

    inline void ResizeBindings(size_t newSize)
    {
        bindings.resize(newSize);
    }
    inline void ResizeAttributes(size_t newSize)
    {
        attributes.resize(newSize);
    }
    inline void ResizeViewports(size_t newSize)
    {
        viewports.resize(newSize);
    }
    inline void ResizeScissors(size_t newSize)
    {
        scissors.resize(newSize);
    }
    inline void ResizeBlendAttachments(size_t newSize)
    {
        blendAttachments.resize(newSize);
    }
    inline void ResizeDynamicStates(size_t newSize)
    {
        dynamicStates.resize(newSize);
    }

    void ReInit(uint32_t shaderStagesCount, uint32_t bindingsCount, uint32_t attributesCount,
        uint32_t viewportsCount, uint32_t scissorsCount, uint32_t blendAttachmentsCount, uint32_t dynamicStatesCount)
    {

        shaderStages.resize(shaderStagesCount);
        bindings.resize(bindingsCount);
        attributes.resize(attributesCount);
        viewports.resize(viewportsCount); 
        scissors.resize(scissorsCount);
        blendAttachments.resize(blendAttachmentsCount);
        dynamicStates.resize(dynamicStatesCount);


        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // в какую геометрию собираем

        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_NONE; // когда показывать грань
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // когда считать что грань повернута лицом

        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT; // отрисовывать честно пиксель

        colorBlendState.logicOpEnable = VK_FALSE; // логические попиксельные операции
    }

    struct PipelineShaderStageCreateInfo : public VkPipelineShaderStageCreateInfo
    {
        PipelineShaderStageCreateInfo() : VkPipelineShaderStageCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO } {}
    };
    static_assert(sizeof(PipelineShaderStageCreateInfo) == sizeof(VkPipelineShaderStageCreateInfo));

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    std::vector<PipelineShaderStageCreateInfo> shaderStages{};

    std::vector<VkVertexInputBindingDescription> bindings{}; // с каким шагом читать
    std::vector<VkVertexInputAttributeDescription> attributes{};
    VkPipelineVertexInputStateCreateInfo vertexInputState{}; // настраиваем все параметры и данные к шейдеру
    
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};

    std::vector<VkViewport> viewports{}; // берем область для отрисовки
    std::vector<VkRect2D> scissors{};
    VkPipelineViewportStateCreateInfo viewportState{};

    VkPipelineRasterizationStateCreateInfo rasterizer{}; // правила отрисовки
    VkPipelineMultisampleStateCreateInfo multisampling{}; // сглаживание геометрии

    std::vector<VkPipelineColorBlendAttachmentState> blendAttachments{}; // решает что делать с наложением 
    VkPipelineColorBlendStateCreateInfo colorBlendState{};

    std::vector<VkDynamicState> dynamicStates{};
    VkPipelineDynamicStateCreateInfo dynamicState{}; // динамические состояния, которые придеться передавать в цикле

    VkPipelineRenderingCreateInfo renderingInfo{}; // pNext
};



































//class VulkanInterfaceCreatePipeline
//{
//public:
//    // sType по умолчанию: Fill его не трогает, новые элементы после resize тоже получают его
//    struct PipelineShaderStageCreateInfo : public VkPipelineShaderStageCreateInfo
//    {
//        PipelineShaderStageCreateInfo() : VkPipelineShaderStageCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO } {}
//    };
//    static_assert(sizeof(PipelineShaderStageCreateInfo) == sizeof(VkPipelineShaderStageCreateInfo));
//
//    // Какую часть вектора берет пайплайн: [offset, offset + count)
//    // count = kAll: все элементы от offset до конца вектора (подстраивается под resize)
//    struct Range
//    {
//        static constexpr uint32_t kAll = ~0u;
//        constexpr Range(uint32_t c = kAll, uint32_t o = 0) : count(c), offset(o) {}
//        uint32_t count;
//        uint32_t offset;
//    };
//
//    VulkanInterfaceCreatePipeline(VulkanInterfaceCreatePipeline&) = delete;
//    VulkanInterfaceCreatePipeline(VulkanInterfaceCreatePipeline&&) = delete;
//
//    VulkanInterfaceCreatePipeline(uint32_t shaderStagesCount, uint32_t bindingsCount, uint32_t attributesCount,
//        uint32_t viewportsCount, uint32_t scissorsCount, uint32_t blendAttachmentsCount, uint32_t dynamicStatesCount)
//        : shaderStages(shaderStagesCount), bindings(bindingsCount), attributes(attributesCount),
//        viewports(viewportsCount), scissors(scissorsCount), blendAttachments(blendAttachmentsCount),
//        dynamicStates(dynamicStatesCount)
//    {
//        pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
//
//        vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
//
//        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
//        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // в какую геометрию собираем
//
//        // VkPipelineViewportStateCreateInfo (говорит с каким пространством и какого размера работаем \ Viewport - задает трансформацию NDC -> экранные координаты, Scisor - обрезает растеризацию)
//        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
//
//        // VkPipelineRasterizationStateCreateInfo ( говорит правила растеризации)
//        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
//        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
//        rasterizer.lineWidth = 1.0f;
//        rasterizer.cullMode = VK_CULL_MODE_NONE; // когда показывать грань
//        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // когда считать что грань повернута лицом
//
//        // VkPipelineMultisampleStateCreateInfo (MSAA)
//        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
//        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT; // отрисовывать честно пиксель
//
//        // VkPipelineColorBlendStateCreateInfo ()
//        colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
//        colorBlendState.logicOpEnable = VK_FALSE; // логические попиксельные операции
//
//        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
//
//        renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
//
//        // нулевая маска каналов означает "ничего не пишется", поэтому слоты сразу получают значения по умолчанию
//        for (uint32_t i = 0; i < blendAttachmentsCount; ++i)
//            FillBlendAttachment(i);
//    }
//
//    inline void SetPushConstants(VkPushConstantRange* pushConstantRanges, uint32_t count = 1)
//    {
//        pipelineLayoutCreateInfo.pushConstantRangeCount = count;
//        pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges;
//    }
//
//    inline void SetPushConstants(VkDescriptorSetLayout* descriptorSetLayouts, uint32_t count = 1)
//    {
//        pipelineLayoutCreateInfo.setLayoutCount = count;
//        pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts;
//    }
//
//    void FillShaderStage(uint32_t idx, VkShaderModule& shaderModule, VkShaderStageFlagBits stageFlag = VK_SHADER_STAGE_VERTEX_BIT, const char* startFuncName = "main")
//    {
//        shaderStages[idx].stage = stageFlag;
//        shaderStages[idx].module = shaderModule;
//        shaderStages[idx].pName = startFuncName;
//    }
//
//    // ---- Vertex input ----
//    void FillBinding(uint32_t idx, uint32_t binding, uint32_t stride, VkVertexInputRate inputRate)
//    {
//        bindings[idx].binding = binding; // номер аппаратного слота
//        bindings[idx].stride = stride; // размер одной вершины
//        bindings[idx].inputRate = inputRate;
//    }
//
//    void FillAttribute(uint32_t idx, uint32_t binding, uint32_t location, uint32_t offset, VkFormat format)
//    {
//        attributes[idx].binding = binding;
//        attributes[idx].location = location;
//        attributes[idx].format = format;
//        attributes[idx].offset = offset;
//    }
//
//    // без аргументов: взять весь вектор
//    inline void SetVertexInputStateBindings(uint32_t count = Range::kAll, uint32_t offset = 0)
//    {
//        bindingsRange = Range(count, offset);
//    }
//    inline void SetVertexInputStateAttributes(uint32_t count = Range::kAll, uint32_t offset = 0)
//    {
//        attributesRange = Range(count, offset);
//    }
//
//    // ---- Viewport ----
//    inline void FillViewport(uint32_t idx, VkExtent2D extent, float x = 0, float y = 0, float minDepth = 0.0, float maxDepth = 1.0)
//    {
//        viewports[idx].x = x;
//        viewports[idx].y = y;
//        viewports[idx].width = static_cast<float>(extent.width);
//        viewports[idx].height = static_cast<float>(extent.height);
//        viewports[idx].minDepth = minDepth;
//        viewports[idx].maxDepth = maxDepth;
//    }
//    inline void FillViewport(uint32_t idx, float width, float height, float x = 0, float y = 0, float minDepth = 0.0, float maxDepth = 1.0)
//    {
//        viewports[idx].x = x;
//        viewports[idx].y = y;
//        viewports[idx].width = width;
//        viewports[idx].height = height;
//        viewports[idx].minDepth = minDepth;
//        viewports[idx].maxDepth = maxDepth;
//    }
//    inline void FillScissor(uint32_t idx, VkExtent2D extent, VkOffset2D offset = { 0, 0 })
//    {
//        scissors[idx].extent = extent;
//        scissors[idx].offset = offset;
//    }
//
//    inline void SetViewportStateViewports(uint32_t count = Range::kAll, uint32_t offset = 0)
//    {
//        viewportsRange = Range(count, offset);
//    }
//    inline void SetViewportStateScissors(uint32_t count = Range::kAll, uint32_t offset = 0)
//    {
//        scissorsRange = Range(count, offset);
//    }
//
//    // ---- Color blend ----
//    // По умолчанию: бленд выключен, пишутся все каналы. Факторы ONE/ZERO и операция ADD дают "результат = источник",
//    // поэтому если включить blendEnable и не трогать остальное, картинка не изменится (а не станет черной)
//    inline void FillBlendAttachment(uint32_t idx,
//        VkBool32              blendEnable = VK_FALSE,
//        VkBlendFactor         srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
//        VkBlendFactor         dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
//        VkBlendOp             colorBlendOp = VK_BLEND_OP_ADD,
//        VkBlendFactor         srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
//        VkBlendFactor         dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
//        VkBlendOp             alphaBlendOp = VK_BLEND_OP_ADD,
//        VkColorComponentFlags colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
//        VK_COLOR_COMPONENT_G_BIT |
//        VK_COLOR_COMPONENT_B_BIT |
//        VK_COLOR_COMPONENT_A_BIT)
//    {
//        blendAttachments[idx].blendEnable = blendEnable;
//        blendAttachments[idx].srcColorBlendFactor = srcColorBlendFactor;
//        blendAttachments[idx].dstColorBlendFactor = dstColorBlendFactor;
//        blendAttachments[idx].colorBlendOp = colorBlendOp;
//        blendAttachments[idx].srcAlphaBlendFactor = srcAlphaBlendFactor;
//        blendAttachments[idx].dstAlphaBlendFactor = dstAlphaBlendFactor;
//        blendAttachments[idx].alphaBlendOp = alphaBlendOp;
//        blendAttachments[idx].colorWriteMask = colorWriteMask;
//    }
//
//    inline void SetColorBlendStateAttachments(uint32_t count = Range::kAll, uint32_t offset = 0)
//    {
//        blendAttachmentsRange = Range(count, offset);
//    }
//
//    // ---- Dynamic state ----
//    inline void FillDynamicState(uint32_t idx, VkDynamicState state)
//    {
//        dynamicStates[idx] = state;
//    }
//    inline void SetDynamicStates(uint32_t count = Range::kAll, uint32_t offset = 0)
//    {
//        dynamicStatesRange = Range(count, offset);
//    }
//
//    // ---- Rendering info ----
//    inline void SetRenderingInfoAttachment(VkFormat* formats, uint32_t count)
//    {
//        renderingInfo.colorAttachmentCount = count;
//        renderingInfo.pColorAttachmentFormats = formats; // данные о цветовой политре surface
//    }
//
//    VkPipeline Create(VkDevice& device)
//    {
//        ApplyRanges(); // счетчики и указатели ставятся здесь, из текущих векторов и Range
//
//        VkPipelineLayout screenPipelineLayout;                                                                                                  // VkPipelineLayout (ресурсы для стадии \ дескрипторы, пуши и тд)
//        vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &screenPipelineLayout);
//
//        // есть еще VkComputePipelineCreateInfo
//        VkGraphicsPipelineCreateInfo pipelineCreateInfo{};                                                                                      // VkGraphicsPipelineCreateInfo (собирает все в один pipeline \ можно указать от какого pipeline отталкиваться при компиляции)
//        pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO; // sType сдесь разный, может быть VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
//        pipelineCreateInfo.pNext = &renderingInfo;
//        pipelineCreateInfo.stageCount = shaderStages.size(); // бедет задействованы два шейдера
//        pipelineCreateInfo.pStages = shaderStages.data(); // шейдеры
//        pipelineCreateInfo.pVertexInputState = &vertexInputState;
//        pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
//        pipelineCreateInfo.pViewportState = &viewportState;
//        pipelineCreateInfo.pRasterizationState = &rasterizer;
//        pipelineCreateInfo.pMultisampleState = &multisampling;
//        pipelineCreateInfo.pColorBlendState = &colorBlendState;
//        pipelineCreateInfo.pDynamicState = &dynamicState;
//        pipelineCreateInfo.layout = screenPipelineLayout;
//        VkPipeline scenePipeline; // создание pipeline
//        if (vkCreateGraphicsPipelines(
//            device,
//            VK_NULL_HANDLE,
//            1,
//            &pipelineCreateInfo,
//            nullptr,
//            &scenePipeline) != VK_SUCCESS)
//        {
//            throw std::runtime_error("Scene pipeline not create");
//        }
//        return scenePipeline;
//    }
//
//    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
//    std::vector<PipelineShaderStageCreateInfo> shaderStages{};
//
//    std::vector<VkVertexInputBindingDescription> bindings{}; // с каким шагом читать
//    Range bindingsRange{};
//    std::vector<VkVertexInputAttributeDescription> attributes{};
//    Range attributesRange{};
//    VkPipelineVertexInputStateCreateInfo vertexInputState{}; // настраиваем все параметры и данные к шейдеру
//
//    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
//
//    std::vector<VkViewport> viewports{}; // берем область для отрисовки
//    Range viewportsRange{};
//    std::vector<VkRect2D> scissors{};
//    Range scissorsRange{};
//    VkPipelineViewportStateCreateInfo viewportState{};
//
//    VkPipelineRasterizationStateCreateInfo rasterizer{}; // правила отрисовки
//    VkPipelineMultisampleStateCreateInfo multisampling{}; // сглаживание геометрии
//
//    std::vector<VkPipelineColorBlendAttachmentState> blendAttachments{}; // решает что делать с наложением
//    Range blendAttachmentsRange{};
//    VkPipelineColorBlendStateCreateInfo colorBlendState{};
//
//    std::vector<VkDynamicState> dynamicStates{};
//    Range dynamicStatesRange{};
//    VkPipelineDynamicStateCreateInfo dynamicState{}; // динамические состояния, которые придеться передавать в цикле
//
//    VkPipelineRenderingCreateInfo renderingInfo{}; // TODO: Это pNext и нужно подумать
//
//private:
//    template<typename T>
//    static uint32_t ResolveCount(const Range& r, const std::vector<T>& v)
//    {
//        assert(r.offset <= v.size());
//        uint32_t count = (r.count == Range::kAll) ? static_cast<uint32_t>(v.size()) - r.offset : r.count;
//        assert(r.offset + count <= v.size());
//        return count;
//    }
//
//    void ApplyRanges()
//    {
//        vertexInputState.vertexBindingDescriptionCount = ResolveCount(bindingsRange, bindings);
//        vertexInputState.pVertexBindingDescriptions = bindings.data() + bindingsRange.offset;
//        vertexInputState.vertexAttributeDescriptionCount = ResolveCount(attributesRange, attributes);
//        vertexInputState.pVertexAttributeDescriptions = attributes.data() + attributesRange.offset;
//
//        viewportState.viewportCount = ResolveCount(viewportsRange, viewports);
//        viewportState.pViewports = viewports.data() + viewportsRange.offset;
//        viewportState.scissorCount = ResolveCount(scissorsRange, scissors);
//        viewportState.pScissors = scissors.data() + scissorsRange.offset;
//
//        colorBlendState.attachmentCount = ResolveCount(blendAttachmentsRange, blendAttachments);
//        colorBlendState.pAttachments = blendAttachments.data() + blendAttachmentsRange.offset;
//
//        dynamicState.dynamicStateCount = ResolveCount(dynamicStatesRange, dynamicStates);
//        dynamicState.pDynamicStates = dynamicStates.data() + dynamicStatesRange.offset;
//    }
//};







// TODO: Глянуть на его реализацию и изменить свою







































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

// TODO: добавить загрузку обычного символьного шейдера (как-то через инструменты вулкана)
VkShaderModule VulkanInterface::LoadShader(std::string path, VkDevice &device, const VkAllocationCallbacks* pAllocator)
    {
        auto vertexShaderCode = ReadBinary(path);

        static VkShaderModuleCreateInfo shaderModuleCreateInfo{};
        shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderModuleCreateInfo.codeSize = vertexShaderCode.size();
        shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(vertexShaderCode.data());

        VkShaderModule shader;
        vkCreateShaderModule(device, &shaderModuleCreateInfo, pAllocator, &shader);
        return shader;
    }

