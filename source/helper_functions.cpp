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
		const vk::Instance vulkanInstance)
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
			.setAllocationSize(std::max(bufferSize, memoryRequirements.size))
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

	void destroy_buffer(
		const vk::Device device,
		vk::Buffer buffer)
	{
		device.destroyBuffer(buffer);
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

	void free_command_buffer(
		const vk::Device device,
		const vk::CommandPool commandPool,
		vk::CommandBuffer commandBuffer)
	{
		device.freeCommandBuffers(commandPool, 1u, &commandBuffer);
	}

	std::tuple<vk::Buffer, vk::DeviceMemory, int, int> load_image_into_host_coherent_buffer(
		const vk::PhysicalDevice physicalDevice,
		const vk::Device device,
		const std::string pathToImageFile)
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

		device.bindBufferMemory(buffer, memory, 0);
		
		// Copy the image's data into the buffer
		auto mappedMemory = device.mapMemory(memory, 0, bufferCreateInfo.size);
		memcpy(mappedMemory, pixels, bufferCreateInfo.size);
		device.unmapMemory(memory);

		stbi_image_free(pixels);

		return std::make_tuple(buffer, memory, width, height);
	}

	void establish_pipeline_barrier_with_image_layout_transition(
		const vk::CommandBuffer commandBuffer,
		const vk::PipelineStageFlags srcPipelineStage, const vk::PipelineStageFlags dstPipelineStage,
		const vk::AccessFlags srcAccessMask, const vk::AccessFlags dstAccessMask,
		const vk::Image image,
		const vk::ImageLayout oldLayout, const vk::ImageLayout newLayout
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
		const vk::CommandBuffer commandBuffer,
		const vk::Buffer buffer,
		const vk::Image image, const uint32_t width, const uint32_t height)
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

	std::tuple<size_t, vk::Buffer, vk::DeviceMemory, vk::Buffer, vk::DeviceMemory, vk::Buffer, vk::DeviceMemory> load_positions_and_texture_coordinates_and_normals_of_obj(
		const std::string modelPath,
		const vk::Device device,
		const vk::PhysicalDevice physicalDevice,
		const std::string submeshNamesToExclude)
	{
		// This code is borrowed from Alexander Overvoorde's Vulkan Tutorial, but has been modified:
		
		tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, modelPath.c_str())) {
            throw std::runtime_error(warn + err);
        }

		std::vector<glm::vec3> positions;
		std::vector<glm::vec2> textureCoordinates;
		std::vector<glm::vec3> normals;
		
        for (const auto& shape : shapes) {
        	if (!submeshNamesToExclude.empty() && shape.name.find("tile") != std::string::npos) {
        		continue;
        	}
        	
            for (const auto& index : shape.mesh.indices) {

            	positions.emplace_back(
					attrib.vertices[3 * index.vertex_index + 0], 
					attrib.vertices[3 * index.vertex_index + 1], 
					attrib.vertices[3 * index.vertex_index + 2]
				);
            	
				textureCoordinates.emplace_back(
					attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
				);

            	normals.emplace_back(
					attrib.normals[3 * index.vertex_index + 0], 
					attrib.normals[3 * index.vertex_index + 1], 
					attrib.normals[3 * index.vertex_index + 2]
				);
            }
        }

		// 1. POSITIONS BUFFER
		// Create the buffer:
		auto posBufferCreateInfo = vk::BufferCreateInfo{}
			.setSize(static_cast<vk::DeviceSize>(sizeof(positions[0]) * positions.size()))
			.setUsage(vk::BufferUsageFlagBits::eVertexBuffer); // <--- Mind the usage flags!
		auto posBuffer = device.createBuffer(posBufferCreateInfo);

		// Allocate backing memory:
		auto posMemory = helpers::allocate_host_coherent_memory_for_given_requirements(physicalDevice, device, 
			posBufferCreateInfo.size,
			device.getBufferMemoryRequirements(posBuffer)
		);

		device.bindBufferMemory(posBuffer, posMemory, 0);
		
		// Copy the positions into the buffer:
		auto posMappedMemory = device.mapMemory(posMemory, 0, posBufferCreateInfo.size);
		memcpy(posMappedMemory, positions.data(), posBufferCreateInfo.size);
		device.unmapMemory(posMemory);

		// 2. TEXTURE COORDINATES BUFFER
		// Create the buffer:
		auto texcoBufferCreateInfo = vk::BufferCreateInfo{}
			.setSize(static_cast<vk::DeviceSize>(sizeof(textureCoordinates[0]) * textureCoordinates.size()))
			.setUsage(vk::BufferUsageFlagBits::eVertexBuffer); // <--- Mind the usage flags!
		auto texcoBuffer = device.createBuffer(texcoBufferCreateInfo);
		
		// Allocate backing memory:
		auto texcoMemory = helpers::allocate_host_coherent_memory_for_given_requirements(physicalDevice, device, 
			texcoBufferCreateInfo.size,
			device.getBufferMemoryRequirements(texcoBuffer)
		);

		device.bindBufferMemory(texcoBuffer, texcoMemory, 0);
		
		// Copy the texture coordinates into the buffer:
		auto texcoMappedMemory = device.mapMemory(texcoMemory, 0, texcoBufferCreateInfo.size);
		memcpy(texcoMappedMemory, textureCoordinates.data(), texcoBufferCreateInfo.size);
		device.unmapMemory(texcoMemory);

		// 2. NORMALS BUFFER
		// Create the buffer:
		auto nrmBufferCreateInfo = vk::BufferCreateInfo{}
			.setSize(static_cast<vk::DeviceSize>(sizeof(normals[0]) * normals.size()))
			.setUsage(vk::BufferUsageFlagBits::eVertexBuffer); // <--- Mind the usage flags!
		auto nrmBuffer = device.createBuffer(nrmBufferCreateInfo);
		
		// Allocate backing memory:
		auto nrmMemory = helpers::allocate_host_coherent_memory_for_given_requirements(physicalDevice, device, 
			nrmBufferCreateInfo.size,
			device.getBufferMemoryRequirements(nrmBuffer)
		);

		device.bindBufferMemory(nrmBuffer, nrmMemory, 0);
		
		// Copy the texture coordinates into the buffer:
		auto nrmMappedMemory = device.mapMemory(nrmMemory, 0, nrmBufferCreateInfo.size);
		memcpy(nrmMappedMemory, normals.data(), nrmBufferCreateInfo.size);
		device.unmapMemory(nrmMemory);

		// Done => return:
		return std::make_tuple(positions.size(), posBuffer, posMemory, texcoBuffer, texcoMemory, nrmBuffer, nrmMemory);
	}

	std::tuple<vk::Image, vk::DeviceMemory> create_image(
		const vk::Device device,
		const vk::PhysicalDevice physicalDevice,
		const uint32_t width, const uint32_t height, const vk::Format format, const vk::ImageUsageFlags usageFlags)
	{
		auto createInfo = vk::ImageCreateInfo{}
			.setImageType(vk::ImageType::e2D)
			.setExtent({width, height, 1u})
			.setMipLevels(1u)
			.setArrayLayers(1u)
			.setFormat(format)
			.setTiling(vk::ImageTiling::eOptimal)			// We just create all images in optimal tiling layout
			.setInitialLayout(vk::ImageLayout::eUndefined)	// Initially, the layout is undefined
			.setUsage(usageFlags)
			.setSamples(vk::SampleCountFlagBits::e1)
			.setSharingMode(vk::SharingMode::eExclusive);
		auto image = device.createImage(createInfo);

		auto memoryRequirements = device.getImageMemoryRequirements(image);

		auto memoryAllocInfo = vk::MemoryAllocateInfo{}
			.setAllocationSize(memoryRequirements.size)
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
						if ((memoryProperties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) // In contrast to our host-coherent buffers, we just assume that we want all our images to live in device memory
								!= vk::MemoryPropertyFlags{}) {
							// Return the INDEX of a suitable memory type
							return i;
						}
					}
					throw std::runtime_error("Couldn't find suitable memory.");
				}());

		auto memory = device.allocateMemory(memoryAllocInfo);

		device.bindImageMemory(image, memory, 0);

		return std::make_tuple(image, memory);
	}

	void destroy_image(
		const vk::Device device,
		vk::Image image)
	{
		device.destroyImage(image);
	}

	vk::ImageView create_image_view(
		const vk::Device device,
		const vk::PhysicalDevice physicalDevice,
		const vk::Image image, const vk::Format format, const vk::ImageAspectFlags imageAspectFlags)
	{
		auto createInfo = vk::ImageViewCreateInfo{}
			.setImage(image)
			.setViewType(vk::ImageViewType::e2D)
			.setFormat(format)
			.setSubresourceRange({imageAspectFlags, 0u, 1u, 0u, 1u});

		auto imageView = device.createImageView(createInfo);

		return imageView;
	}

	void destroy_image_view(
		const vk::Device device,
		vk::ImageView imageView)
	{
		device.destroyImageView(imageView);
	}

	std::tuple<vk::ShaderModule, vk::PipelineShaderStageCreateInfo> load_shader_and_create_shader_module_and_stage_info(
		const vk::Device device,
		const std::string path,
		const vk::ShaderStageFlagBits shaderStage)
	{
		// This code is borrowed from Alexander Overvoorde's Vulkan Tutorial, it has been modified slightly:
		
		std::ifstream file(path, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("failed to open file!");
        }

        size_t fileSize = (size_t) file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);

        file.close();

        auto moduleCreateInfo = vk::ShaderModuleCreateInfo{}
			.setCodeSize(buffer.size())
			.setPCode(reinterpret_cast<const uint32_t*>(buffer.data()));

        auto shaderModule = device.createShaderModule(moduleCreateInfo);

		auto shaderStageCreateInfo = vk::PipelineShaderStageCreateInfo{}
			.setStage(shaderStage)
			.setModule(shaderModule)
			.setPName("main"); // entry point
		
        return std::make_tuple(shaderModule, shaderStageCreateInfo);
	}

	void destroy_shader_module(
		const vk::Device device,
		vk::ShaderModule shaderModule)
	{
		device.destroyShaderModule(shaderModule);
	}

	std::tuple<vk::Buffer, vk::DeviceMemory> create_host_coherent_buffer_and_memory(
		const vk::Device device,
		const vk::PhysicalDevice physicalDevice,
		const size_t bufferSize, 
		const vk::BufferUsageFlags bufferUsageFlags)
	{
		// Describe a new buffer:
		auto createInfo = vk::BufferCreateInfo{}
			.setSize(static_cast<vk::DeviceSize>(bufferSize))
			.setUsage(bufferUsageFlags);
		auto buffer = device.createBuffer(createInfo);

		// Allocate the memory (we want host-coherent memory):
		auto memory = helpers::allocate_host_coherent_memory_for_given_requirements(physicalDevice, device, createInfo.size, device.getBufferMemoryRequirements(buffer));

		// Bind the buffer handle to the memory:
		device.bindBufferMemory(buffer, memory, 0); 

		return std::make_tuple(buffer, memory);
	}

	void copy_data_into_host_coherent_memory(
		const vk::Device device,
		const size_t dataSize,
		const void* data, 
		vk::DeviceMemory memory)
	{
		auto clearColorMappedMemory = device.mapMemory(memory, 0, dataSize);
		memcpy(clearColorMappedMemory, data, dataSize);
		device.unmapMemory(memory);
	}
	
}
