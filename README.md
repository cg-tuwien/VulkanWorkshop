# Vulkan Workshop

Vulkan Workshop, created for the CESCG 2020

Vulkan is a new (relatively) new low-level API from The Khronos Group. It is a completely different type of API than OpenGL. Not only is it more modern, it is much more explicit and notably lower-level than OpenGL. A programmer needs to take care of many things that weren't required to be taken care of with OpenGL. 

This workshop shall introduce programmers to the basic concepts of the Vulkan API and offers several units where programmers can jump right into the action, without requiring huge initial setup efforts.

Accompanying videos to the individual workshop units will be made available on YouTube soon.

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
  * STB image ([`external/stb`](external/stb))
  * tiny obj loader ([`external/tinyobj`](external/tinyobj))
  * Vulkan SDK's include directory (you'll probably want to use the `VULKAN_SDK` environment variable with `/Include` appended)
* A custom (post-)build step which performs the following actions (or do it manually): 
  * Copy all images from [`resources/images`](resources/images) to the target directory's subfolder `images/`
  * Compile shader files using the `glslc` compiler (can be found `VULKAN_SDK`'s `Bin`-subdirectory) as follows (where "resources" refers to [`resources/`](resources) and "targetdirectory" refers to your build output directory):
    * `glslc -c resources/shaders/vertex_shader.vert -o targetdirectory/shaders/vertex_shader.spv`
    * `glslc -c resources/shaders/fragment_shader.frag -o targetdirectory/shaders/fragment_shader.spv`
    
In short, the code will try to load images from relative paths `images/*`, and shader files from relative paths `shaders/*`.
    
## Workshop Units

The individual workshop units are listed below. Each unit's initial code version is available on a separate branch. The idea is that you switch to the branch of a unit, check it out, and start working on its tasks.
* Unit 01: First Steps ([Branch `unit_01`](https://github.com/cg-tuwien/VulkanWorkshop/tree/unit_01))

---

**External resources/dependencies and their licenses**
* GLFW (zlib/libpng license) [`external/glfw`](external/glfw)
* stb_image (MIT license or Public Domain) [`external/stb`](external/stb)
* tiny obj loader (MIT license) [`external/tinyobj`](external/tinyobj)
* [Free VFX Image Sequences & Flipbooks](https://blogs.unity3d.com/pt/2016/11/28/free-vfx-image-sequences-flipbooks) by Thomas ICHÉ (CC0 license) [`resources/images`](resources/images)
