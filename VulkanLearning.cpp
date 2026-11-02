
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
    auto [deviceHandle, allocator] = CD.Create();

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

    VulkanInterfaceCreateBuffer createBuffer(sizeof(Vertex) * vertices.size());
    auto [vertexBuffer, vertexAllocation] = createBuffer.Create(vertices.data(), sizeof(Vertex) * vertices.size(), allocator);
    createBuffer.NewBuffer(sizeof(uint32_t) * indices.size());
    auto [indexBuffer, indexAllocation] = createBuffer.Create(indices.data(), sizeof(uint32_t) * indices.size(), allocator);

    //////////////////////////////
    //  Загрузка кода шейдеров  //
    //////////////////////////////

    VkShaderModule sceneVertexShader = VulkanInterface::LoadShader("C:\\all\\proj\\VisualStudio\\VulkanLearning\\shaders\\scene.vert.spv", deviceHandle);
    VkShaderModule sceneFragmentShader = VulkanInterface::LoadShader("C:\\all\\proj\\VisualStudio\\VulkanLearning\\shaders\\scene.frag.spv", deviceHandle);
    VkShaderModule screenVertexShader = VulkanInterface::LoadShader("C:\\all\\proj\\VisualStudio\\VulkanLearning\\shaders\\screen.vert.spv", deviceHandle);
    VkShaderModule screenFragmentShader = VulkanInterface::LoadShader("C:\\all\\proj\\VisualStudio\\VulkanLearning\\shaders\\screen.frag.spv", deviceHandle);

    //////////////////////////////////
    // Биндим картинку в descriptor //
    //////////////////////////////////

    VulkanInterfaceCreateDescriptorSetLayout createDescriptorSetLayout{ 1 };
    createDescriptorSetLayout.FillBindings();
    VkDescriptorSetLayout descriptorSetLayout = createDescriptorSetLayout.Create(deviceHandle);

    VulkanInterfaceCreateDescriptorPool createDescriptorPool;
    createDescriptorPool.FillDescriptionPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1);
    VkDescriptorPool descriptorPool = createDescriptorPool.Create(deviceHandle);

    VulkanInterfaceCreateDescriptorSet createDescriptorSet(descriptorPool, &descriptorSetLayout);
    VkDescriptorSet descriptorSet = createDescriptorSet.CreateOne(deviceHandle);

    VulkanInterfaceWriteDescriptorSet writeDescriptorSet(1, 0, 0, 1, 0);
    writeDescriptorSet.FillDescriptorImageInfo(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, offscreenImageView, offscreenSampler);
    writeDescriptorSet.FillDescriptorSet(0, descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, 0, 1);
    writeDescriptorSet.Update(deviceHandle);

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

    VulkanInterfaceCreatePipeline createPipeline(2, 1, 2, 1, 1, 1, 2); // мейби передавать топологию
    createPipeline.SetPushConstants(&pushConstantRange, 1);

    createPipeline.FillShaderStage(0, sceneVertexShader, VK_SHADER_STAGE_VERTEX_BIT);
    createPipeline.FillShaderStage(1, sceneFragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT);

    createPipeline.FillBinding(0, 0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX);
    createPipeline.FillAttribute(0, 0, 0, offsetof(Vertex, position), VK_FORMAT_R32G32_SFLOAT);
    createPipeline.FillAttribute(1, 0, 1, offsetof(Vertex, color), VK_FORMAT_R32G32B32_SFLOAT);
    
    createPipeline.FillViewport(0, extent);
    createPipeline.FillScissor(0, extent);
    
    // Нужна функция под заполненеи rasterizer и multisampling

    createPipeline.FillBlendAttachment(0);

    createPipeline.dynamicStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
    createPipeline.dynamicStates[1] = VK_DYNAMIC_STATE_SCISSOR;

    createPipeline.SetRenderingInfoFormats(&surfaceFormat.format, 1);

    auto [scenePipelineLayout, scenePipeline] = createPipeline.Create(deviceHandle);



    createPipeline.SetPushConstants(nullptr, 0);
    createPipeline.SetDescriptorSetLayouts(&descriptorSetLayout, 1);

    createPipeline.FillShaderStage(0, screenVertexShader, VK_SHADER_STAGE_VERTEX_BIT);
    createPipeline.FillShaderStage(1, screenFragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT);

    createPipeline.ResizeAttributes(0);
    createPipeline.ResizeBindings(0);

    auto [screenPipelineLayout, screenPipeline] = createPipeline.Create(deviceHandle);

    
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
