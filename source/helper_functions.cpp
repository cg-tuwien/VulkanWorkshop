#include "pch.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

namespace helpers
{
	GLFWwindow* create_window_with_glfw(
		const int windowWidth, 
		const int windowHeight)
	{
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "Vulkan window", nullptr, nullptr);
		return window;
	}

	vk::Instance create_vulkan_instance_with_validation_layers()
	{
		static const std::vector<const char*> EnabledVkValidationLayers = {
			"VK_LAYER_KHRONOS_validation"
		};

		uint32_t numGlfwExtensions;
		auto glfwExtensions = glfwGetRequiredInstanceExtensions(&numGlfwExtensions);
		auto instCreateInfo = vk::InstanceCreateInfo{}
			.setEnabledExtensionCount(static_cast<uint32_t>(numGlfwExtensions))
			.setPpEnabledExtensionNames(glfwExtensions)
			.setEnabledLayerCount(static_cast<uint32_t>(EnabledVkValidationLayers.size()))
			.setPpEnabledLayerNames(EnabledVkValidationLayers.data());
		
		auto vkInstance = vk::createInstance(instCreateInfo);
		return vkInstance;
	}

	VkSurfaceKHR create_surface(
		GLFWwindow* window, 
		vk::Instance vulkanInstance)
	{
		VkSurfaceKHR surface;
		if (VK_SUCCESS != glfwCreateWindowSurface(vulkanInstance, window, nullptr, &surface)) {
			throw std::runtime_error("Couldn't create surface");
		}
		return surface;
	}

	uint32_t find_queue_family_index_for_parameters(
		const vk::PhysicalDevice physicalDevice,
		const VkSurfaceKHR surfaceToBeSupported,
		const vk::QueueFlags operationsToBeSupported)
	{
		auto familyProps = physicalDevice.getQueueFamilyProperties();
		for (uint32_t i = 0; i < familyProps.size(); ++i) {
			// Test for surface support:
			if (physicalDevice.getSurfaceSupportKHR(i, surfaceToBeSupported) == VK_FALSE) {
				continue;
			}
			// Test for operations support:
			if ((familyProps[i].queueFlags & operationsToBeSupported) != vk::QueueFlags{}) {
				// Return the INDEX of a suitable queue family
				return i;
			}
		}
		throw std::runtime_error("Couldn't find a suitable queue family");
	}

	vk::Device create_logical_device(
		const vk::PhysicalDevice physicalDevice,
		const VkSurfaceKHR surfaceToBeSupported)
	{
		static const std::vector<const char*> EnabledVkDeviceExtensions = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME
		};

		// Look for a queue family which supports:
		//  - the surface, and
		//  - all sorts of queue operations (i.e. graphics, compute, and transfer)
		//  
		// Afterwards, when creating the logical device, request ONE such a queue to be created!
		// 
		float queuePriority = 1.0f;
		auto queueCreateInfo = vk::DeviceQueueCreateInfo{}
			.setQueueCount(1u)
			.setQueueFamilyIndex(helpers::find_queue_family_index_for_parameters(
				physicalDevice, 
				surfaceToBeSupported, 
				vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute | vk::QueueFlagBits::eTransfer
			))
			.setPQueuePriorities(&queuePriority);

		// Create a logical device which is an interface to the physical device
		// and also request a queue to be created
		auto deviceCreateInfo = vk::DeviceCreateInfo{}
			.setQueueCreateInfoCount(1u)
			.setPQueueCreateInfos(&queueCreateInfo)
			.setEnabledExtensionCount(static_cast<uint32_t>(EnabledVkDeviceExtensions.size()))
			.setPpEnabledExtensionNames(EnabledVkDeviceExtensions.data());
		auto device = physicalDevice.createDevice(deviceCreateInfo);

		return device;
	}

	std::tuple<uint32_t, vk::Queue> get_queue_on_logical_device(
		const vk::PhysicalDevice physicalDevice,
		const VkSurfaceKHR surfaceToBeSupported,
		const vk::Device logcialDevice)
	{
		auto queueFamilyIndex = helpers::find_queue_family_index_for_parameters(
			physicalDevice, 
			surfaceToBeSupported, 
			vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute | vk::QueueFlagBits::eTransfer
		);
		return std::make_tuple(queueFamilyIndex, logcialDevice.getQueue(queueFamilyIndex, 0u));
	}

	vk::DeviceMemory allocate_host_coherent_memory_for_given_requirements(
		const vk::PhysicalDevice physicalDevice,
		const vk::Device device,
		const vk::DeviceSize bufferSize,
		const vk::MemoryRequirements memoryRequirements)
	{
		auto memoryAllocInfo = vk::MemoryAllocateInfo{}
			.setAllocationSize(bufferSize)
			.setMemoryTypeIndex([&]() {
					// Get memory types supported by the physical device:
					auto memoryProperties = physicalDevice.getMemoryProperties();
			
					// In search for a suitable memory type INDEX:
					for (uint32_t i = 0u; i < memoryProperties.memoryTypeCount; ++i) {
						
						// Is this kind of memory suitable for our buffer?
						const auto bitmask = memoryRequirements.memoryTypeBits;
						const auto bit = 1 << i;
						if (0 == (bitmask & bit)) {
							continue; // => nope
						}
						
						// Does this kind of memory support our usage requirements?
						if ((memoryProperties.memoryTypes[i].propertyFlags & (vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent))
								!= vk::MemoryPropertyFlags{}) {
							// Return the INDEX of a suitable memory type
							return i;
						}
					}
					throw std::runtime_error("Couldn't find suitable memory.");
				}());

		// Allocate:
		auto memory = device.allocateMemory(memoryAllocInfo);
		
		return memory;
	}

	void free_memory(
		const vk::Device device,
		vk::DeviceMemory memory)
	{
		device.freeMemory(memory);
	}

	vk::CommandBuffer allocate_command_buffer(
		const vk::Device device,
		const vk::CommandPool commandPool)
	{
		auto allocInfo = vk::CommandBufferAllocateInfo{}
    		.setCommandBufferCount(1u)
    		.setCommandPool(commandPool);
		return device.allocateCommandBuffers(allocInfo)[0];
	}

	std::tuple<vk::Buffer, vk::DeviceMemory, int, int> load_image_into_host_coherent_buffer(
		const vk::PhysicalDevice physicalDevice,
		const vk::Device device,
		std::string pathToImageFile)
	{
		const int desiredColorChannels = STBI_rgb_alpha;	
		int width, height, channelsInFile;
		stbi_uc* pixels = stbi_load(pathToImageFile.c_str(), &width, &height, &channelsInFile, desiredColorChannels); 
		size_t imageDataSize = width * height * desiredColorChannels;

		// Convert RGB -> BGR
		// TODO: Not sure if this is a good idea on all different GPUs. Probably it's not. If the image looks odd => try something else here.
		for (int i = 0; i < imageDataSize; i += desiredColorChannels) {
			stbi_uc tmp = pixels[i];
			pixels[i] = pixels[i+2];
			pixels[i+2] = tmp;
		}

		// Create a buffer:
		auto bufferCreateInfo = vk::BufferCreateInfo{}
			.setSize(static_cast<vk::DeviceSize>(imageDataSize))
			.setUsage(vk::BufferUsageFlagBits::eTransferSrc);
		auto buffer = device.createBuffer(bufferCreateInfo);
		
		// Create the memory:
		auto memory = helpers::allocate_host_coherent_memory_for_given_requirements(physicalDevice, device, 
			bufferCreateInfo.size,
			device.getBufferMemoryRequirements(buffer)
		);
		
		// Copy the image's data into the buffer
		auto clearColorMappedMemory = device.mapMemory(memory, 0, bufferCreateInfo.size);
		memcpy(clearColorMappedMemory, pixels, bufferCreateInfo.size);
		device.unmapMemory(memory);

		stbi_image_free(pixels);

		return std::make_tuple(buffer, memory, width, height);
	}

	void establish_pipeline_barrier_with_image_layout_transition(
		vk::CommandBuffer commandBuffer,
		vk::PipelineStageFlags srcPipelineStage, vk::PipelineStageFlags dstPipelineStage,
		vk::AccessFlags srcAccessMask, vk::AccessFlags dstAccessMask,
		vk::Image image,
		vk::ImageLayout oldLayout, vk::ImageLayout newLayout
	)
	{
		auto imageMemoryBarrier = vk::ImageMemoryBarrier{};
		imageMemoryBarrier.srcAccessMask = srcAccessMask;
		imageMemoryBarrier.dstAccessMask = dstAccessMask;
		imageMemoryBarrier.oldLayout = oldLayout;
		imageMemoryBarrier.newLayout = newLayout;
		imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageMemoryBarrier.image = image;
		imageMemoryBarrier.subresourceRange = vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

		commandBuffer.pipelineBarrier(
			srcPipelineStage,
			dstPipelineStage,
			{}, {}, {},
			{ imageMemoryBarrier }
		);
	}

	void copy_buffer_to_image(
		vk::CommandBuffer commandBuffer,
		vk::Buffer buffer,
		vk::Image image, uint32_t width, uint32_t height)
	{
		commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, { 
			vk::BufferImageCopy{
				0, width, height,
				vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, 0, 0, 1}, vk::Offset3D{0, 0, 0}, vk::Extent3D{width, height, 1}
			}
		});
	}

	void destroy_window(GLFWwindow* window)
	{
	    glfwDestroyWindow(window);
	}
	
	void destroy_vulkan_instance(vk::Instance vulkanInstance)
	{
		vulkanInstance.destroy();
	}
	
	void destroy_surface(const vk::Instance vulkanInstance, VkSurfaceKHR surface)
	{
		vulkanInstance.destroy(surface);
	}
	
	void destroy_logical_device(vk::Device device)
	{
		device.destroy();
	}
}
