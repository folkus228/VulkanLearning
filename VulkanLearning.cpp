
#include "VulkanInterface.h"

struct Vertex
{
    glm::vec2 position;
    glm::vec3 color;
};

struct PushConstant
{
    glm::mat4 transform;
    float time;
};

int main()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Vulkan", nullptr, nullptr); // создали окно

        ////////////////////////////////////////
        // Инициализация и подготовка Vulkan  //
        ////////////////////////////////////////

    InitVulkanInterface initVulkanInterface;
    initVulkanInterface.applicationInfo.pApplicationName = "Example";
    initVulkanInterface.applicationInfo.pEngineName = "None";
    VulkanInterface::Init(initVulkanInterface, window);

    std::vector<VkPhysicalDevice> devices = VulkanInterface::GetDevices();
    for (auto& device : devices)
        PrintDeviceData(device);
    VkPhysicalDevice physicalDevice = devices[0]; // TODO: искать девайс по возможностям и семействам которые на нем есть

    uint32_t graphicsQueueFamily = 0; // запомним номер графического семейства
    std::vector<VkQueueFamilyProperties> queueFamilys = VulkanInterface::GetDeviceQueueFamilys(physicalDevice);

    for (uint32_t i = 0; i < queueFamilys.size(); i++)
    {
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, VulkanInterface::mainSurface, &presentSupport);  // поддержка работы с нашим окном
        //                           VkQueueFlagBits
        if ((queueFamilys[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport) // поддержка работы с графикой и окном
        {
            graphicsQueueFamily = i;
            break;
        }
    }

    VulkanInterfaceCreateDevice CD(physicalDevice, 1, graphicsQueueFamily);
    VkPhysicalDeviceDynamicRenderingFeatures dynamicRendering{};
    dynamicRendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynamicRendering.dynamicRendering = VK_TRUE;
    CD.deviceCreateInfo.pNext = &dynamicRendering;
    const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME }; // запрос на конкретно отрисовку пикселей на экране
    CD.SetEnabledExtensions(deviceExtensions, 1);
    CD.ApplyPrioritys();
    VkDevice deviceHandle = CD.CreateDevice();
    VmaAllocator allocator = CD.CreateAllocator();

    VkQueue graphicsQueue = VulkanInterface::CreateQueue(deviceHandle, graphicsQueueFamily); // наша графическая очередь (будем отправлять команды через нее)
    VkSurfaceCapabilitiesKHR surfaceCapabilities = VulkanInterface::CreateSurfaceCapabilities(physicalDevice, VulkanInterface::mainSurface); // возможности работы с монитором (min и max разрешение и поддерживоемое кол-во буфферов)

    VkSurfaceKHR& surface = VulkanInterface::mainSurface;

    std::vector<VkSurfaceFormatKHR> surfaceFormats = VulkanInterface::GetSurfaceFormats(physicalDevice, surface);
    VkSurfaceFormatKHR surfaceFormat = surfaceFormats[0]; // ? должен быть цикл с выбором лучшего
    for (const auto& format : surfaceFormats)
    {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            surfaceFormat = format;
            break;
        }
    }

    std::vector<VkPresentModeKHR> presentModes = VulkanInterface::GetPresentModes(physicalDevice, surface);
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR; // базовый v-sync
    for (auto mode : presentModes)
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) // тройная буферизация и Fast Sync
        {
            presentMode = mode;
            break;
        }

    VkExtent2D extent = VulkanInterface::CreateExtent2D(surfaceCapabilities);

    VulkanInterfaceCreateSwapchain createSwapchein(surfaceCapabilities, surface, surfaceFormat);
    VkSwapchainKHR swapchain = createSwapchein.CreateSwapchain(deviceHandle);
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    createSwapchein.FillViews(swapchainImages, swapchainImageViews, swapchain, deviceHandle);

    VulkanInterfaceCreateCommandPool createCommandPool(graphicsQueueFamily, swapchainImages.size());
    VkCommandPool commandPool = createCommandPool.CreateCommandPool(deviceHandle);
    std::vector<VkCommandBuffer> commandBuffers = createCommandPool.CreateCommandBuffers(deviceHandle);

    VulkanInterfaceCreateSemaphore createSemaphore;
    VkSemaphore imageAvailableSemaphore = createSemaphore.Create(deviceHandle);
    VkSemaphore renderFinishedSemaphore = createSemaphore.Create(deviceHandle);

    VulkanInterfaceCreateFence createFence;
    VkFence frameFence = createFence.Create(deviceHandle);

    VulkanInterfaceCreateImage createImage(extent.width, extent.height, surfaceFormat.format);
    auto [offscreenImage, offscreenImageView, offscreenAllocation] = createImage.Create(deviceHandle, allocator);

    VulkanInterfaceCreateSampler createSampler;
    VkSampler offscreenSampler = createSampler.Create(deviceHandle);


    ////////////////////////////////
    // Логика работы с вершинами  //
    ////////////////////////////////
    std::vector<Vertex> vertices =
    {
        {{ 0.000000f,  0.700000f }, {1.0f,0.0f,0.0f}},
        {{ 0.665740f,  0.216312f }, {0.0f,1.0f,0.0f}},
        {{ 0.411450f, -0.566312f }, {0.0f,0.0f,1.0f}},
        {{-0.411450f, -0.566312f }, {1.0f,1.0f,0.0f}},
        {{-0.665740f,  0.216312f }, {1.0f,0.0f,1.0f}}
    };
    std::vector<uint32_t> indices =
    {
        0,1,2,
        0,2,3,
        0,3,4
    };

    VkBufferCreateInfo vertexBufferCreateInfo{};
    vertexBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertexBufferCreateInfo.size = sizeof(Vertex) * vertices.size();
    vertexBufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    vertexBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo vertexAllocInfo{};
    vertexAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    vertexAllocInfo.flags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | // пишем последовательно и CPU не может читать
        VMA_ALLOCATION_CREATE_MAPPED_BIT; // открыть память для прямой записи

    VkBuffer vertexBuffer;
    VmaAllocation vertexAllocation; // просто handl выделенной памяти
    if (vmaCreateBuffer(allocator, &vertexBufferCreateInfo, &vertexAllocInfo,
        &vertexBuffer, &vertexAllocation, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create vertex buffer via VMA");
    }
    VmaAllocationInfo vertexAllocData; // конкретные данные выделенной памяти
    vmaGetAllocationInfo(allocator, vertexAllocation, &vertexAllocData);
    memcpy(vertexAllocData.pMappedData, vertices.data(), sizeof(Vertex) * vertices.size());

    ///////////////////////

    VkBufferCreateInfo indexBufferCreateInfo{};
    indexBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    indexBufferCreateInfo.size = sizeof(uint32_t) * indices.size();
    indexBufferCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    indexBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;


    VmaAllocationCreateInfo indexAllocInfo{};
    indexAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    indexAllocInfo.flags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBuffer indexBuffer;
    VmaAllocation indexAllocation;
    if (vmaCreateBuffer(allocator, &indexBufferCreateInfo, &indexAllocInfo,
        &indexBuffer, &indexAllocation, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create index buffer via VMA");
    }
    VmaAllocationInfo indexAllocData;
    vmaGetAllocationInfo(allocator, indexAllocation, &indexAllocData);
    memcpy(indexAllocData.pMappedData, indices.data(), sizeof(uint32_t) * indices.size());

    //////////////////////////////
    //  Загрузка кода шейдеров  //
    //////////////////////////////
    auto vertexShaderCode = ReadBinary("C:\\all\\proj\\VisualStudio\\VulkanLearning\\shaders\\scene.vert.spv");
    auto fragmentShaderCode = ReadBinary("C:\\all\\proj\\VisualStudio\\VulkanLearning\\shaders\\scene.frag.spv");

    VkShaderModuleCreateInfo shaderModuleCreateInfo{};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.codeSize = vertexShaderCode.size();
    shaderModuleCreateInfo.pCode = reinterpret_cast<
        const uint32_t*>(vertexShaderCode.data());

    VkShaderModule sceneVertexShader;
    vkCreateShaderModule(deviceHandle, &shaderModuleCreateInfo, nullptr, &sceneVertexShader);

    shaderModuleCreateInfo.codeSize = fragmentShaderCode.size();
    shaderModuleCreateInfo.pCode = reinterpret_cast
        <const uint32_t*>(fragmentShaderCode.data());

    VkShaderModule sceneFragmentShader;
    vkCreateShaderModule(deviceHandle, &shaderModuleCreateInfo, nullptr, &sceneFragmentShader);


    auto screenVertexShaderCode = ReadBinary("C:\\all\\proj\\VisualStudio\\VulkanLearning\\shaders\\screen.vert.spv");
    auto screenFragmentShaderCode = ReadBinary("C:\\all\\proj\\VisualStudio\\VulkanLearning\\shaders\\screen.frag.spv");

    shaderModuleCreateInfo.codeSize = screenVertexShaderCode.size();
    shaderModuleCreateInfo.pCode = reinterpret_cast<
        const uint32_t*>(screenVertexShaderCode.data());

    VkShaderModule screenVertexShader;
    vkCreateShaderModule(deviceHandle, &shaderModuleCreateInfo, nullptr, &screenVertexShader);

    shaderModuleCreateInfo.codeSize = screenFragmentShaderCode.size();
    shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(
        screenFragmentShaderCode.data());

    VkShaderModule screenFragmentShader;
    vkCreateShaderModule(deviceHandle, &shaderModuleCreateInfo, nullptr, &screenFragmentShader);

    //////////////////////////////////
    // Биндим картинку в descriptor //
    //////////////////////////////////

    /////////////// Дескриптор для чтения данных шейдером
    VkDescriptorSetLayoutBinding samplerBinding{}; // описание одного слота
    samplerBinding.binding = 0;
    samplerBinding.descriptorCount = 1;
    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.bindingCount = 1;
    descriptorSetLayoutCreateInfo.pBindings = &samplerBinding;
    VkDescriptorSetLayout descriptorSetLayout; // штука описывает одно поле layout
    vkCreateDescriptorSetLayout(deviceHandle, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout);

    VkDescriptorPoolSize descriptorPoolSize{};
    descriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorPoolSize.descriptorCount = 1;
    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
    descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.maxSets = 1;
    descriptorPoolCreateInfo.poolSizeCount = 1;
    descriptorPoolCreateInfo.pPoolSizes = &descriptorPoolSize;
    VkDescriptorPool descriptorPool; // место для каждого layout (VkDescriptorSetLayout)
    vkCreateDescriptorPool(deviceHandle, &descriptorPoolCreateInfo, nullptr, &descriptorPool);

    VkDescriptorSetAllocateInfo descriptorAllocateInfo{};
    descriptorAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorAllocateInfo.descriptorPool = descriptorPool;
    descriptorAllocateInfo.descriptorSetCount = 1;
    descriptorAllocateInfo.pSetLayouts = &descriptorSetLayout;
    VkDescriptorSet descriptorSet; // объединяет контейнер и все описания layout'ов
    vkAllocateDescriptorSets(deviceHandle, &descriptorAllocateInfo, &descriptorSet);


    /////////////// Дескриптор для записи данных в поле
    VkDescriptorImageInfo descriptorImageInfo{};
    descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    descriptorImageInfo.imageView = offscreenImageView;
    descriptorImageInfo.sampler = offscreenSampler;
    VkWriteDescriptorSet writeDescriptorSet{}; // штука для связывания данных с местом в шейдере
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.dstSet = descriptorSet;       // 1. Куда пишем? В нашу пустую коробочку-сет
    writeDescriptorSet.dstBinding = 0;               // 2. В какую розетку пишем? В биндинг №0
    writeDescriptorSet.dstArrayElement = 0;          // 3. С какого элемента массива начать? С нулевого
    writeDescriptorSet.descriptorCount = 1;          // 4. Сколько розеток заполняем за раз? Одну
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; // 5. Тип того, что вставляем
    writeDescriptorSet.pImageInfo = &descriptorImageInfo; // 6. Ссылка на саму текстуру и её сглаживатель
    vkUpdateDescriptorSets(deviceHandle, 1, &writeDescriptorSet, 0, nullptr);

    /////////////////////////////////////
    // создаем быструю передачу данных //
    /////////////////////////////////////
    VkPushConstantRange pushConstantRange{}; // быстрый регистр на 128 байт
    pushConstantRange.offset = 0; // отступ в этом регистре
    pushConstantRange.size = sizeof(PushConstant); // размер
    pushConstantRange.stageFlags =  // каким шейдерам позволяется читать эти данные
        VK_SHADER_STAGE_VERTEX_BIT |
        VK_SHADER_STAGE_FRAGMENT_BIT;

    ////////////////////////////////////
    //  Создание конвеера рендеринга  //
    ////////////////////////////////////
    VkPipelineLayoutCreateInfo scenePipelineLayoutCreateInfo{}; // первый конвеер, кладем данные нужные для этапа
    scenePipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    scenePipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    scenePipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
    VkPipelineLayout scenePipelineLayout;                                                                                                   // VkPipelineLayout (определяет ресурсы \ пуш константы и дэскрипторы)
    vkCreatePipelineLayout(deviceHandle, &scenePipelineLayoutCreateInfo, nullptr, &scenePipelineLayout);

    VkPipelineLayoutCreateInfo screenPipelineLayoutCreateInfo{}; // второй конвеер
    screenPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    screenPipelineLayoutCreateInfo.setLayoutCount = 1;
    screenPipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
    VkPipelineLayout screenPipelineLayout;                                                                                                  // VkPipelineLayout (ресурсы для стадии \ дескрипторы, пуши и тд)
    vkCreatePipelineLayout(deviceHandle, &screenPipelineLayoutCreateInfo, nullptr, &screenPipelineLayout);

    VkPipelineShaderStageCreateInfo shaderStages[2]{}; // указываем стадии рендеринга                                                       // VkPipelineShaderStageCreateInfo (работа на стадии шейдеров \ передача констант, точки входа, тип)
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = sceneVertexShader;
    shaderStages[0].pName = "main";
    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = sceneFragmentShader;
    shaderStages[1].pName = "main";

    VkVertexInputBindingDescription binding{}; // с каким шагом читать
    binding.binding = 0; // номер аппаратного слота
    binding.stride = sizeof(Vertex); // размер одной вершины
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributes[2]{}; // описывает каждую часть вершины и подвязывает к слотам шейдера
    // Атрибут 0: Позиция
    attributes[0].binding = 0;
    attributes[0].location = 0;
    attributes[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributes[0].offset = offsetof(Vertex, position);
    // Атрибут 1: Цвет
    attributes[1].binding = 0;
    attributes[1].location = 1;
    attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[1].offset = offsetof(Vertex, color);

    VkPipelineVertexInputStateCreateInfo vertexInputState{}; // настраиваем все параметры и данные к шейдеру                                // VkPipelineVertexInputStateCreateInfo (че делать с вершинами \ шаги, типы переменных, к каким слотам биндить)
    vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputState.vertexBindingDescriptionCount = 1;
    vertexInputState.pVertexBindingDescriptions = &binding;
    vertexInputState.vertexAttributeDescriptionCount = 2;
    vertexInputState.pVertexAttributeDescriptions = attributes;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};                                                                                 // VkPipelineInputAssemblyStateCreateInfo (как собирать примитивы \ какая геометрия будет на выходе)
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // в какую геометрию собираем


    VkViewport viewport{}; // берем область для отрисовки
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.extent = extent;

    VkPipelineViewportStateCreateInfo viewportState{};                                                                                      // VkPipelineViewportStateCreateInfo (говорит с каким пространством и какого размера работаем \ Viewport - задает трансформацию NDC -> экранные координаты, Scisor - обрезает растеризацию)
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{}; // правила отрисовки                                                               // VkPipelineRasterizationStateCreateInfo ( говорит правила растеризации)
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE; // когда показывать грань
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // когда считать что грань повернута лицом

    VkPipelineMultisampleStateCreateInfo multisampling{}; // сглаживание геометрии                                                          // VkPipelineMultisampleStateCreateInfo (MSAA)
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT; // отрисовывать честно пиксель

    // VkPipelineDepthStencilStateCreateInfo ()

    VkPipelineColorBlendAttachmentState blendAttachment{}; // решает что делать с наложением                                                // VkPipelineColorBlendAttachmentState (блендинг (один на каждый атачмент?))
    blendAttachment.colorWriteMask = // дает доступ к перезаписи каналов
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    blendAttachment.blendEnable = VK_FALSE; // смешивание цветов

    VkPipelineColorBlendStateCreateInfo colorBlendState{};                                                                                  // VkPipelineColorBlendStateCreateInfo ()
    colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendState.logicOpEnable = VK_FALSE; // логические попиксельные операции
    colorBlendState.attachmentCount = 1; // настройка цвета только для одной текстуры
    colorBlendState.pAttachments = &blendAttachment;

    VkDynamicState dynamicStates[] = // подключаемые динамические фичи
    {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState{}; // динамические состояния, которые придеться передавать в цикле                        // VkPipelineDynamicStateCreateInfo (что вынести в runtime)
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkPipelineRenderingCreateInfo renderingInfo{};                                                                                          // ?
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &surfaceFormat.format; // данные о цветовой политре surface

    // есть еще VkComputePipelineCreateInfo
    VkGraphicsPipelineCreateInfo pipelineCreateInfo{};                                                                                      // VkGraphicsPipelineCreateInfo (собирает все в один pipeline \ можно указать от какого pipeline отталкиваться при компиляции)
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.pNext = &renderingInfo;
    pipelineCreateInfo.stageCount = 2; // бедет задействованы два шейдера
    pipelineCreateInfo.pStages = shaderStages; // шейдеры
    pipelineCreateInfo.pVertexInputState = &vertexInputState;
    pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pRasterizationState = &rasterizer;
    pipelineCreateInfo.pMultisampleState = &multisampling;
    pipelineCreateInfo.pColorBlendState = &colorBlendState;
    pipelineCreateInfo.pDynamicState = &dynamicState;
    pipelineCreateInfo.layout = scenePipelineLayout;
    VkPipeline scenePipeline; // создание pipeline
    if (vkCreateGraphicsPipelines(
        deviceHandle,
        VK_NULL_HANDLE,
        1,
        &pipelineCreateInfo,
        nullptr,
        &scenePipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("Scene pipeline");
    }

    shaderStages[0].module = screenVertexShader; // меняем шейдеры под второй конвеер
    shaderStages[1].module = screenFragmentShader;
    VkPipelineVertexInputStateCreateInfo screenVertexInput{};
    screenVertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    pipelineCreateInfo.pVertexInputState = &screenVertexInput;
    pipelineCreateInfo.layout = screenPipelineLayout;

    VkPipeline screenPipeline;
    if (vkCreateGraphicsPipelines(
        deviceHandle,
        VK_NULL_HANDLE,
        1,
        &pipelineCreateInfo,
        nullptr,
        &screenPipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("Screen pipeline");
    }

    vkDestroyShaderModule(deviceHandle, sceneVertexShader, nullptr);
    vkDestroyShaderModule(deviceHandle, sceneFragmentShader, nullptr);
    vkDestroyShaderModule(deviceHandle, screenVertexShader, nullptr);
    vkDestroyShaderModule(deviceHandle, screenFragmentShader, nullptr);

    auto start = std::chrono::high_resolution_clock::now();

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        vkWaitForFences(deviceHandle, 1, &frameFence, VK_TRUE, UINT64_MAX);

        vkResetFences(deviceHandle, 1, &frameFence);

        uint32_t imageIndex;

        vkAcquireNextImageKHR(deviceHandle, swapchain, UINT64_MAX,
            imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

        VkCommandBuffer commandBuffer = commandBuffers[imageIndex];

        vkResetCommandBuffer(commandBuffer, 0);

        VkCommandBufferBeginInfo beginInfo{};

        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.image = offscreenImage;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier);

        VkRenderingAttachmentInfo colorAttachment{};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView = offscreenImageView;
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

        VkClearValue clear{};
        clear.color = { {0.05f,0.05f,0.05f,1.0f} };

        colorAttachment.clearValue = clear;

        VkRenderingInfo renderInfo{};
        renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderInfo.renderArea.extent = extent;
        renderInfo.layerCount = 1;
        renderInfo.colorAttachmentCount = 1;
        renderInfo.pColorAttachments = &colorAttachment;

        vkCmdBeginRendering(commandBuffer, &renderInfo);
        VkViewport dynamicViewport{};
        dynamicViewport.x = 0.0f;
        dynamicViewport.y = 0.0f;
        dynamicViewport.width = static_cast<float>(extent.width);
        dynamicViewport.height = static_cast<float>(extent.height);
        dynamicViewport.minDepth = 0.0f;
        dynamicViewport.maxDepth = 1.0f;

        vkCmdSetViewport(commandBuffer, 0, 1, &dynamicViewport);

        VkRect2D dynamicScissor{};
        dynamicScissor.offset = { 0,0 };
        dynamicScissor.extent.width = extent.width;
        dynamicScissor.extent.height = extent.height;

        vkCmdSetScissor(commandBuffer, 0, 1, &dynamicScissor);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, scenePipeline);

        VkDeviceSize vertexOffset = 0;
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, &vertexOffset);

        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        auto now = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float>(now - start).count();
        PushConstant pushConstant{};
        pushConstant.time = time;
        pushConstant.transform = glm::rotate(
            glm::mat4(1.0f),
            time,
            glm::vec3(0.0f, 0.0f, 1.0f));

        vkCmdPushConstants(
            commandBuffer,
            scenePipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT |
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(PushConstant),
            &pushConstant);

        vkCmdDrawIndexed(
            commandBuffer,
            static_cast<uint32_t>(indices.size()),
            1,
            0,
            0,
            0);

        vkCmdEndRendering(commandBuffer);

        barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier);

        VkImageMemoryBarrier swapchainBarrier{};
        swapchainBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        swapchainBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        swapchainBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        swapchainBarrier.srcAccessMask = 0;
        swapchainBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        swapchainBarrier.image = swapchainImages[imageIndex];
        swapchainBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        swapchainBarrier.subresourceRange.baseMipLevel = 0;
        swapchainBarrier.subresourceRange.levelCount = 1;
        swapchainBarrier.subresourceRange.baseArrayLayer = 0;
        swapchainBarrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &swapchainBarrier);

        colorAttachment.imageView = swapchainImageViews[imageIndex];
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        clear.color = { {0.0f,0.0f,0.0f,1.0f} };
        colorAttachment.clearValue = clear;

        vkCmdBeginRendering(commandBuffer, &renderInfo);

        vkCmdSetViewport(commandBuffer, 0, 1, &dynamicViewport);

        vkCmdSetScissor(commandBuffer, 0, 1, &dynamicScissor);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, screenPipeline);

        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            screenPipelineLayout,
            0,
            1,
            &descriptorSet,
            0,
            nullptr);

        vkCmdDraw(commandBuffer, 3, 1, 0, 0);

        vkCmdEndRendering(commandBuffer);


        swapchainBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        swapchainBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        swapchainBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        swapchainBarrier.dstAccessMask = 0;
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &swapchainBarrier);

        vkEndCommandBuffer(commandBuffer);

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &imageAvailableSemaphore;
        submitInfo.pWaitDstStageMask = &waitStage;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &renderFinishedSemaphore;

        vkQueueSubmit(graphicsQueue, 1, &submitInfo, frameFence);

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchain;
        presentInfo.pImageIndices = &imageIndex;
        vkQueuePresentKHR(graphicsQueue, &presentInfo);
    }

    vkDeviceWaitIdle(deviceHandle);
    vkDestroyPipeline(deviceHandle, scenePipeline, nullptr);
    vkDestroyPipeline(deviceHandle, screenPipeline, nullptr);
    vkDestroyPipelineLayout(deviceHandle, scenePipelineLayout, nullptr);
    vkDestroyPipelineLayout(deviceHandle, screenPipelineLayout, nullptr);
    vkDestroyDescriptorPool(deviceHandle, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(deviceHandle, descriptorSetLayout, nullptr);
    vkDestroySampler(deviceHandle, offscreenSampler, nullptr);
    vkDestroyImageView(deviceHandle, offscreenImageView, nullptr);
    // VMA сам удалит и ресурсы, и их внутреннюю память
    vmaDestroyImage(allocator, offscreenImage, offscreenAllocation);
    vmaDestroyBuffer(allocator, vertexBuffer, vertexAllocation);
    vmaDestroyBuffer(allocator, indexBuffer, indexAllocation);
    vkDestroySemaphore(deviceHandle, imageAvailableSemaphore, nullptr);
    vkDestroySemaphore(deviceHandle, renderFinishedSemaphore, nullptr);
    vkDestroyFence(deviceHandle, frameFence, nullptr);
    vkDestroyCommandPool(deviceHandle, commandPool, nullptr);
    for (VkImageView imageView : swapchainImageViews)
        vkDestroyImageView(deviceHandle, imageView, nullptr);
    vkDestroySwapchainKHR(deviceHandle, swapchain, nullptr);
    vmaDestroyAllocator(allocator);
    vkDestroyDevice(deviceHandle, nullptr);
    VulkanInterface::Shutdown();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
