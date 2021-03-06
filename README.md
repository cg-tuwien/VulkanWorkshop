# Vulkan Workshop

Vulkan Workshop, originally created for the [CESCG 2020](https://cescg.org), made available for everyone.

Developed and presented by [Johannes Unterguggenberger](https://www.cg.tuwien.ac.at/staff/JohannesUnterguggenberger.html), University Assistant at the [Research Unit of Computer Graphics](https://www.cg.tuwien.ac.at/), Institute of Visual Computing & Human-Centered Technology, TU Wien, Austria.

**This workshop** is intended for programmers who have already some experience with C++ and an older/higher-level graphics API like OpenGL. It will teach some of the most important concepts and usages of the low-level Vulkan graphics API. Depending on your working style and previous experience, a very rough estimation is that you will be able to complete the whole workshop in approximately 2 days (+/- 1 day). 

**Vulkan** is a (relatively) new graphics API from The Khronos Group. It is a completely different type of API than OpenGL. Not only is it more modern, it is much more explicit and notably lower-level than OpenGL. A programmer needs to take care of many things that weren't required to be taken care of with OpenGL. 

This workshop shall introduce programmers to the basic concepts of the Vulkan API and offers several parts where programmers can jump right into the action, without requiring huge initial setup efforts.

For some of the workshop parts, accompanying videos are available. For further details, please navigate to the different parts below.

## Setup and Prerequisites

Please prepare your PC by installing the [LunarG Vulkan SDK](https://vulkan.lunarg.com/sdk/home). Ensure that the `VULKAN_SDK` environment variable is set and loaded (log out and back in, or restart your PC).

This repository is pre-configured for usage with Visual Studio 2019 on Windows, but it should be possible to set it up on other operating systems with relatively little effort. 

### Windows Setup 

Besides the Vulkan SDK, the following is recommended
* Visual Studio 2019 (Community Edition is fine)
* An installed Windows 10 SDK (use the Visual Studio Installer)
* Open [`visual_studio\vk_workshop.sln`](visual_studio), it should build and run right out of the box.

### Manual Setup

If you would like to set-up the project for a different environment than the pre-configured one, you'll need the following:
* Install the Vulkan SDK (as mentioned above)
* Link the library GLFW (see [`external/glfw`](external/glfw))
* Provide the following include directories:
  * GLFW include ([`external/glfw/include`](external/glfw/include))
  * GLM include ([`external/glm`](external/glm))
  * STB image ([`external/stb`](external/stb))
  * tiny obj loader ([`external/tinyobj`](external/tinyobj))
  * Vulkan SDK's include directory (you'll probably want to use the `VULKAN_SDK` environment variable with `/Include` appended)
* A custom (post-)build step which performs the following actions (or do it manually): 
  * Copy all images from [`resources/images`](resources/images) to the target directory's subfolder `images/`
  * Copy all files (models and images) from [`resources/models`](resources/models) to the target directory's subfolder `models/`
  * Compile shader files using the `glslc` compiler (can be found `VULKAN_SDK`'s `Bin`-subdirectory) as follows (where "resources" refers to [`resources/`](resources) and "targetdirectory" refers to your build output directory):
    * `glslc -c resources/shaders/vertex_shader.vert -o targetdirectory/shaders/vertex_shader.spv`
    * `glslc -c resources/shaders/fragment_shader.frag -o targetdirectory/shaders/fragment_shader.spv`
    
In short, the code will try to load images from relative paths `images/*`, models from relative paths `models/*`, and shader files from relative paths `shaders/*`. Shaders must be compiled to SPIR-V.

**Includes**   
For all required `#include` statements, please make sure to include everything that is included in [`source/pch.h`](source/pch.h)! You can probably just include `pch.h`.

## About the code of this workshop

Modern C++ is used throughout this workshop's code.

**Vulkan-Hpp**     
This tutorial uses Vulkan-Hpp which are C++ bindings for the Vulkan C API. It is a matter of taste, but they can make development a bit more convenient. 

The names translate 1:1 between the C API interface and the Vulkan-Hpp interface. Some examples: 
* C API: `vkCreateInstance` <-> Vulkan-Hpp: `vk::createInstance`
* C API: `vkCreateDevice(physicalDevice, ...)` <-> Vulkan-Hpp: `physicalDevice.createDevice(...)`
* C API: `vkCreateSemaphore(device, ...)` <-> Vulkan-Hpp: `device.createSemaphore(...)`
* C API: `vkCmdPipelineBarrier(commandBuffer, ...)` <-> Vulkan-Hpp: `commandBuffer.pipelineBarrier(...)`
* C API: `vkCmdCopyBufferToImage(commandBuffer, ...)` <-> Vulkan-Hpp: `commandBuffer.copyBufferToImage(...)`

For more information, please refer to [KhronosGroup/Vulkan-Hpp](https://github.com/KhronosGroup/Vulkan-Hpp).

*Note:* The [Vulkan specification](https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html) is referring to the C API naming always. I.e. whenever you look something up in the specification, make sure to translate into C naming style!

**Shaders**     
There are two shader files included in this workshop. In later parts ([Part 5](https://github.com/cg-tuwien/VulkanWorkshop/tree/part5) and [Part 6](https://github.com/cg-tuwien/VulkanWorkshop/tree/part6)) they have to be modified as well. You can find them under [`resources/shaders`](resources/shaders). 

If you are using the preconfigured Visual Studio solution, just edit them in place and when you are done editing, simply build the solution in Visual Studio. The configured custom build step will compile the shaders and deploy them to the target directory (if they compiled successfully). If there was an compilation error, take a look at Visual Studio's `Output` view, the error messages should be displayed there.

## Workshop Units

The individual workshop units are listed below. Each unit's initial code version is available on a separate branch. The idea is that you switch to the branch of a unit, check it out, and start working on its tasks.
* [Part 1: First Steps](https://github.com/cg-tuwien/VulkanWorkshop/tree/part1)
* [Part 2: Reusable Command Buffers](https://github.com/cg-tuwien/VulkanWorkshop/tree/part2)
* [Part 3: Concurrent Frames](https://github.com/cg-tuwien/VulkanWorkshop/tree/part3)
* [Part 4: Renderpass and Graphics Pipeline](https://github.com/cg-tuwien/VulkanWorkshop/tree/part4)
* [Part 5: Descriptors and Textures](https://github.com/cg-tuwien/VulkanWorkshop/tree/part5)
* [Part 6: Reflect on it](https://github.com/cg-tuwien/VulkanWorkshop/tree/part6)
