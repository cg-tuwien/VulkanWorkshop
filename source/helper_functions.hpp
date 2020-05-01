#pragma once

namespace helpers
{
	// Create a window using GLFW
	GLFWwindow* create_window_with_glfw(
		const int windowWidth, 
		const int windowHeight
	);

	// Destroy a window that has been created with CreateWindowWithGlfw
	void destroy_window(GLFWwindow* window);
	
	// Initialize Vulkan, and request enable the standard validation layers
	vk::Instance create_vulkan_instance_with_validation_layers();

	// Destroy a vulkan instance that has been created with CreateVulkanInstanceWithValidationLayers
	void destroy_vulkan_instance(vk::Instance vulkanInstance);

	// Create a surface to draw on
	VkSurfaceKHR create_surface(
		GLFWwindow* window, 
		const vk::Instance vulkanInstance
	);

	// Create a surface that has been created with CreateSurface
	void destroy_surface(const vk::Instance vulkanInstance, VkSurfaceKHR surface);

	// For the given queue flags, find a suitable queue family
	uint32_t find_queue_family_index_for_parameters(
		const vk::PhysicalDevice physicalDevice,
		const VkSurfaceKHR surfaceToBeSupported,
		const vk::QueueFlags operationsToBeSupported
	);

	// Create a logical device, which will serve as our interface to a physical device
	vk::Device create_logical_device(
		const vk::PhysicalDevice physicalDevice,
		const VkSurfaceKHR surfaceToBeSupported
	);

	// Destroy a logical device that has been created with CreateLogicalDevice
	void destroy_logical_device(vk::Device device);

	// Get a queue where we can submit commands to
	std::tuple<uint32_t, vk::Queue> get_queue_on_logical_device(
		const vk::PhysicalDevice physicalDevice,
		const VkSurfaceKHR surfaceToBeSupported,
		const vk::Device logcialDevice
	);

	// Allocate a command buffer from the given command pool
	vk::CommandBuffer allocate_command_buffer(
		const vk::Device device,
		const vk::CommandPool commandPool
	);

	// Free command buffers that have been created with one of the helper functions
	void free_command_buffer(
		const vk::Device device,
		const vk::CommandPool commandPool,
		vk::CommandBuffer commandBuffer
	);
	
	// Allocate "host coherent" memory, which is accessible from both, the CPU-side (host) and the GPU-side (device).
	// !! Host coherent memory will automatically be made available on the the device on queue-submits !!
	vk::DeviceMemory allocate_host_coherent_memory_for_given_requirements(
		const vk::PhysicalDevice physicalDevice,
		const vk::Device device,
		const vk::DeviceSize bufferSize,
		const vk::MemoryRequirements memoryRequirements
	);

	// Load an image from a file, and copy it into a newly created buffer (backed with memory already):
	// Returns a tuple with: <0> the buffer handle, <1> the memory handle, <2> width, <3> height
	std::tuple<vk::Buffer, vk::DeviceMemory, int, int> load_image_into_host_coherent_buffer(
		const vk::PhysicalDevice physicalDevice,
		const vk::Device device,
		const std::string pathToImageFile
	);

	// Free memory that has been allocated with AllocateHostCoherentMemoryForBuffer
	void free_memory(
		const vk::Device device,
		vk::DeviceMemory memory
	);

	// Destroy buffers that have been created with one of the helper functions
	void destroy_buffer(
		const vk::Device device,
		vk::Buffer buffer
	);

	// Records a pipeline barrier INCLUDING an image layout transition into the given command buffer.
	// The following parameters are used to define the pipeline barrier:
	//  - srcPipelineStage
	//  - dstPipelineStage
	//  - srcAccessMask
	//  - dstAccessMask
	// The following parameters are used to describe an image layout transition:
	//  - image
	//  - oldLayout
	//  - newLayout
	//  
	// This is a convenience function. Feel free to manually create the barrier using vkCmdPipelineBarrier.
	// 
	// For further details, please read up about the parameters in the specification
	//  - vkCmdPipelineBarrier: https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdPipelineBarrier.html
	//  - VkImageMemoryBarrier: https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkImageMemoryBarrier.html
	//  - image layout transitions: https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html#synchronization-image-layout-transitions
	// 
	void establish_pipeline_barrier_with_image_layout_transition(
		const vk::CommandBuffer commandBuffer,
		const vk::PipelineStageFlags srcPipelineStage, const vk::PipelineStageFlags dstPipelineStage,
		const vk::AccessFlags srcAccessMask, const vk::AccessFlags dstAccessMask,
		const vk::Image image,
		const vk::ImageLayout oldLayout, const vk::ImageLayout newLayout
	);

	// Record copying a buffer to an image into the given command buffer.
	// !! This function assumes the image to be in vk::ImageLayout::eTransferDstOptimal layout !!
	//
	// This is a convenience function. Feel free to manually perform the copy using vkCmdCopyBufferToImage.
	// 
	void copy_buffer_to_image(
		const vk::CommandBuffer commandBuffer,
		const vk::Buffer buffer,
		const vk::Image image, const uint32_t width, const uint32_t height
	);

	// Loads the given 3D .obj model from file, and load its positions (vec3) and texture
	// coordinates (vec2) into two newly created, host-coherent buffers.
	// Returs a tuple containing:
	//  <0>: The number of vertices that the loaded model consists of and that have been stored into the buffers
	//  <1>: the buffer handle to the positions buffer
	//  <2>: memory handle to the position buffer's backing memory
	//  <3>: the buffer handle to the texture coordinates buffer
	//  <4>: memory handle to the texture coordinates buffer's backing memory
	std::tuple<size_t, vk::Buffer, vk::DeviceMemory, vk::Buffer, vk::DeviceMemory> load_positions_and_texture_coordinates_of_obj(
		const std::string modelPath,
		const vk::Device device,
		const vk::PhysicalDevice physicalDevice,
		const std::string submeshNamesToExclude = ""
	);

	// Creates a new image with backing memory
	// Returns a tuple containing <0>: the image handle, <1>: the handle to the image's memory
	std::tuple<vk::Image, vk::DeviceMemory> create_image(
		const vk::Device device,
		const vk::PhysicalDevice physicalDevice,
		const uint32_t width, const uint32_t height, const vk::Format format, const vk::ImageUsageFlags usageFlags
	);

	// Destroy an image that has been created using the helper functions
	void destroy_image(
		const vk::Device device,
		vk::Image image
	);

	// Create an image view to an image
	vk::ImageView create_image_view(
		const vk::Device device,
		const vk::PhysicalDevice physicalDevice,
		const vk::Image image, const vk::Format format, const vk::ImageAspectFlags imageAspectFlags
	);

	// Destroy an image view that has been created using the helper functions
	void destroy_image_view(
		const vk::Device device,
		vk::ImageView imageView
	);

	// Load a shader from file and create a shader module
	std::tuple<vk::ShaderModule, vk::PipelineShaderStageCreateInfo> load_shader_and_create_shader_module_and_stage_info(
		const vk::Device device,
		const std::string path,
		const vk::ShaderStageFlagBits shaderStage
	);

	// Destroy a shader module that has been created using the helper functions
	void destroy_shader_module(
		const vk::Device device,
		vk::ShaderModule shaderModule
	);

	// Create a host coherent buffer with backing memory
	std::tuple<vk::Buffer, vk::DeviceMemory> create_host_coherent_buffer_and_memory(
		const vk::Device device,
		const vk::PhysicalDevice physicalDevice,
		const size_t bufferSize, 
		const vk::BufferUsageFlags bufferUsageFlags
	);

	// Copy data of the gifen size into the buffer
	void copy_data_into_host_coherent_memory(
		const vk::Device device,
		const size_t dataSize,
		const void* data, 
		vk::DeviceMemory memory
	);
	
}
