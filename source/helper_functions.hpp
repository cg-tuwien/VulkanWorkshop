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
		vk::Instance vulkanInstance
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
		std::string pathToImageFile
	);

	// Free memory that has been allocated with AllocateHostCoherentMemoryForBuffer
	void free_memory(
		const vk::Device device,
		vk::DeviceMemory memory
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
		vk::CommandBuffer commandBuffer,
		vk::PipelineStageFlags srcPipelineStage, vk::PipelineStageFlags dstPipelineStage,
		vk::AccessFlags srcAccessMask, vk::AccessFlags dstAccessMask,
		vk::Image image,
		vk::ImageLayout oldLayout, vk::ImageLayout newLayout
	);

	// Record copying a buffer to an image into the given command buffer.
	// !! This function assumes the image to be in vk::ImageLayout::eTransferDstOptimal layout !!
	//
	// This is a convenience function. Feel free to manually perform the copy using vkCmdCopyBufferToImage.
	// 
	void copy_buffer_to_image(
		vk::CommandBuffer commandBuffer,
		vk::Buffer buffer,
		vk::Image image, uint32_t width, uint32_t height
	);

}
