# CMakeList.txt: проект CMake для VulkanLearning; включите исходный код и определения,
# укажите здесь логику для конкретного проекта.
#
cmake_minimum_required(VERSION 3.22)

# Включение горячей перезагрузки для компиляторов MSVC, если поддерживается.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "vcpkg toolchain")

project ("VulkanLearning")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(VCPKG_INSTALLED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/vcpkg") # куда класть либы


# Находим Vulkan SDK (он подтянет заголовочные файлы)
find_package(Vulkan REQUIRED)

# Находим зависимости из vcpkg
find_package(volk CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(unofficial-shaderc CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)


# Добавьте источник в исполняемый файл этого проекта.
add_executable (VulkanLearning "VulkanLearning.cpp" )

target_link_libraries(VulkanLearning PRIVATE 
    glfw
    glm::glm
    volk::volk
    unofficial::shaderc::shaderc
    GPUOpen::VulkanMemoryAllocator
)