#pragma once

namespace helpers
{
	GLFWwindow* CreateWindowWithGlfw(
		const int windowWidth, 
		const int windowHeight
	);

	vk::Instance CreateVulkanInstance();

	VkSurfaceKHR CreateSurface(
		GLFWwindow* window, 
		vk::Instance vulkanInstance
	);

	uint32_t FindQueueFamilyIndexForParameters(
		const vk::PhysicalDevice physicalDevice,
		const VkSurfaceKHR surfaceToBeSupported,
		const vk::QueueFlags operationsToBeSupported
	);

	vk::Device CreateLogicalDevice(
		const vk::PhysicalDevice physicalDevice,
		const VkSurfaceKHR surfaceToBeSupported
	);

	std::tuple<uint32_t, vk::Queue> GetQueueOnLogicalDevice(
		const vk::PhysicalDevice physicalDevice,
		const VkSurfaceKHR surfaceToBeSupported,
		const vk::Device logcialDevice
	);

	vk::DeviceMemory Allocate_host_coherent_memory_and_bind_Memory_to_Buffer(
		const vk::PhysicalDevice physicalDevice,
		const vk::Device device,
		const vk::DeviceSize bufferSize,
		const vk::Buffer buffer
	);

	void Free_Memory(
		const vk::Device device,
		vk::DeviceMemory memory
	);

	std::tuple<vk::Buffer, vk::DeviceMemory, int, int> Load_image_into_host_coherent_Buffer(
		const vk::PhysicalDevice physicalDevice,
		const vk::Device device,
		std::string pathToImageFile
	);

	void RecordBarrierWithImageLayoutTransition(
		vk::CommandBuffer commandBuffer,
		vk::Image image,
		vk::PipelineStageFlags srcPipelineStage, vk::PipelineStageFlags dstPipelineStage,
		vk::AccessFlags srcAccessMask, vk::AccessFlags dstAccessMask,
		vk::ImageLayout oldLayout, vk::ImageLayout newLayout
	);

	void RecordCopyBufferToImage(
		vk::CommandBuffer commandBuffer,
		vk::Buffer buffer,
		vk::Image image, uint32_t width, uint32_t height
	);

	void DestroyWindow(GLFWwindow* window);
	void DestroyVulkanInstance(vk::Instance vulkanInstance);
	void DestroySurface(const vk::Instance vulkanInstance, VkSurfaceKHR surface);
	void DestroyLogicalDevice(vk::Device device);
}
