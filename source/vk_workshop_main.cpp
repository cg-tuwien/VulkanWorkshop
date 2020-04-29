#include "pch.h"

int main()
{
    glfwInit();

	const int width = 800;
	const int height = 800;
	// ===> 1. Create a window
	auto window = helpers::CreateWindowWithGlfw(width, height);
	// ===> 2. Create a vulkan instance
	auto vkInst = helpers::CreateVulkanInstance();
	// ===> 3. Create a surface to draw into
	auto surface = helpers::CreateSurface(window, vkInst);
	// ===> 4. Get a handle to (one) physical device, i.e. to the GPU
    auto physicalDevice = vkInst.enumeratePhysicalDevices().front();
	// ===> 5. Create a logical device which serves as this application's interface to the physical device
	auto device = helpers::CreateLogicalDevice(physicalDevice, surface);	
	// ===> 6. Get a queue on the logical device so we can send commands to it
	auto [queueFamilyIndex, queue] = helpers::GetQueueOnLogicalDevice(physicalDevice, surface, device);

	// ===> 7. Create a swapchain
	auto availableFormats = physicalDevice.getSurfaceFormatsKHR(surface);
	auto availablePresentModes = physicalDevice.getSurfacePresentModesKHR(surface);
	auto swapchainCreateInfo = vk::SwapchainCreateInfoKHR{}
		.setSurface(surface)
		.setImageExtent(vk::Extent2D(width, height))
		.setMinImageCount(2u)
		.setImageArrayLayers(1u)
		.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst)
		.setImageFormat(availableFormats[0].format)
		.setImageColorSpace(availableFormats[0].colorSpace)
		.setPresentMode(availablePresentModes[0]);
	auto swapchain = device.createSwapchainKHR(swapchainCreateInfo);

	// ===> 8. Get all the swapchain's images
	auto swapchainImages = device.getSwapchainImagesKHR(swapchain);

	// TODO: We'd like clear our swapchainImages to a color, i.e. ~glClearColor

	// ===> 9. Define memory for a clear color and put it into a buffer
	// data:
	std::vector<uint8_t> clearColorData;
	clearColorData.resize(width * height * 4);
	for (size_t i = 0; i < clearColorData.size(); i += 4) {
		clearColorData[i + 0] =   0;
		clearColorData[i + 1] =   0;
		clearColorData[i + 2] = 255;
		clearColorData[i + 3] = 255;
	}
	// buffer:
	auto clearColorBufferCreateInfo = vk::BufferCreateInfo{}
		.setSize(static_cast<vk::DeviceSize>(clearColorData.size() * sizeof(clearColorData[0])))
		.setUsage(vk::BufferUsageFlagBits::eTransferSrc);
	auto clearColorBuffer = device.createBuffer(clearColorBufferCreateInfo);
	// buffer MEMORY (we want host-coherent memory):
	auto clearColorMemory = helpers::Allocate_host_coherent_memory_and_bind_Memory_to_Buffer(physicalDevice, device, 
		clearColorBufferCreateInfo.size,
		clearColorBuffer
	);
	// copy our data into the buffer:
	auto clearColorMappedMemory = device.mapMemory(clearColorMemory, 0, clearColorBufferCreateInfo.size);
	memcpy(clearColorMappedMemory, clearColorData.data(), clearColorBufferCreateInfo.size);
	device.unmapMemory(clearColorMemory);

	// Now, we need a command to copy that clearColorData into our swapchain images
	
	// ===> 10. Create a command pool so that we can create commands
	auto commandPoolCreateInfo = vk::CommandPoolCreateInfo{}
		.setQueueFamilyIndex(queueFamilyIndex);
	auto commandPool = device.createCommandPool(commandPoolCreateInfo);

	// ===> 11. Create copy commands, one for each image
    auto commandBufferAllocateInfo = vk::CommandBufferAllocateInfo{}
    	.setCommandBufferCount(swapchainImages.size())
    	.setCommandPool(commandPool);
    auto commandBuffers = device.allocateCommandBuffers(commandBufferAllocateInfo);
    for (size_t i = 0; i < swapchainImages.size(); ++i) {
		commandBuffers[i].begin(vk::CommandBufferBeginInfo{});

    	helpers::RecordBarrierWithImageLayoutTransition(commandBuffers[i], swapchainImages[i],
			vk::PipelineStageFlagBits::eTopOfPipe,	/* -> */ vk::PipelineStageFlagBits::eTransfer,
			vk::AccessFlags{},						/* -> */ vk::AccessFlagBits::eTransferWrite,
    		vk::ImageLayout::eUndefined,			/* -> */ vk::ImageLayout::eTransferDstOptimal
		);

    	helpers::RecordCopyBufferToImage(commandBuffers[i], clearColorBuffer, swapchainImages[i], width, height);

    	helpers::RecordBarrierWithImageLayoutTransition(commandBuffers[i], swapchainImages[i],
			vk::PipelineStageFlagBits::eTransfer,	/* -> */ vk::PipelineStageFlagBits::eAllCommands,
			vk::AccessFlagBits::eMemoryWrite,		/* -> */ vk::AccessFlagBits::eMemoryRead,
    		vk::ImageLayout::eTransferDstOptimal,	/* -> */ vk::ImageLayout::ePresentSrcKHR
		);
    	
    	commandBuffers[i].end();
    }

    auto submitInfo = vk::SubmitInfo{}
    	.setCommandBufferCount(commandBuffers.size())
		.setPCommandBuffers(commandBuffers.data());
	queue.submit({ submitInfo }, nullptr);

	device.waitIdle();

	auto imageAvailableSemaphore = device.createSemaphore(vk::SemaphoreCreateInfo{});
	auto renderFinishedSemaphore = device.createSemaphore(vk::SemaphoreCreateInfo{});
	
	// Load 100 animation files from file
	std::vector<std::tuple<vk::Buffer, vk::DeviceMemory, int, int>> xplosionBuffers;
	for (int i = 1; i <= 100; ++i) {
		auto iToS = "00" + std::to_string(i);
		xplosionBuffers.push_back(helpers::Load_image_into_host_coherent_Buffer(physicalDevice, device, "images/explosion02HD-frame" + iToS.substr(iToS.length() - 3) + ".tga"));
	}

	const double startTime = glfwGetTime();
	const double aniDuration = 0.16666;
	double nextAniTime = startTime + aniDuration;
	int aniIndex = 0;
	std::vector<vk::CommandBuffer> keepAlive;
	
    while(!glfwWindowShouldClose(window)) {
		device.waitIdle();

    	auto curTime = glfwGetTime();
    	if (nextAniTime > curTime) {
    		aniIndex = std::min(aniIndex + 1, static_cast<int>(xplosionBuffers.size()) - 1);
    		nextAniTime = curTime + aniDuration;
    	}
    	
		auto currentSwapchainImageIndex = device.acquireNextImageKHR(swapchain, std::numeric_limits<uint64_t>::max(), imageAvailableSemaphore, nullptr).value;
    	auto& currentSwapchainImage = swapchainImages[currentSwapchainImageIndex];

		auto cb = device.allocateCommandBuffers(vk::CommandBufferAllocateInfo{commandPool, vk::CommandBufferLevel::ePrimary, 1u})[0];
    	cb.begin(vk::CommandBufferBeginInfo{});
		helpers::RecordBarrierWithImageLayoutTransition(cb, currentSwapchainImage,
			vk::PipelineStageFlagBits::eTopOfPipe,	/* -> */ vk::PipelineStageFlagBits::eTransfer,
			vk::AccessFlags{},						/* -> */ vk::AccessFlagBits::eTransferWrite,
    		vk::ImageLayout::eUndefined,			/* -> */ vk::ImageLayout::eTransferDstOptimal
		);
		helpers::RecordCopyBufferToImage(cb, std::get<vk::Buffer>(xplosionBuffers[aniIndex]), currentSwapchainImage, width, height);
    	helpers::RecordBarrierWithImageLayoutTransition(cb, currentSwapchainImage,
			vk::PipelineStageFlagBits::eTransfer,	/* -> */ vk::PipelineStageFlagBits::eBottomOfPipe,
			vk::AccessFlagBits::eTransferWrite,		/* -> */ vk::AccessFlags{},
    		vk::ImageLayout::eTransferDstOptimal,	/* -> */ vk::ImageLayout::ePresentSrcKHR
		);
    	cb.end();

    	vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eAllCommands;
    	auto si = vk::SubmitInfo{}
    		.setCommandBufferCount(1u)
    		.setPCommandBuffers(&cb)
    		.setWaitSemaphoreCount(1u)
    		.setPWaitSemaphores(&imageAvailableSemaphore)
    		.setSignalSemaphoreCount(1u)
    		.setPSignalSemaphores(&renderFinishedSemaphore)
    		.setPWaitDstStageMask(&waitStage);
		queue.submit({ si }, nullptr);
    	
    	auto presentInfo = vk::PresentInfoKHR{}
    		.setSwapchainCount(1)
    		.setPSwapchains(&swapchain)
			.setPImageIndices(&currentSwapchainImageIndex)
    		.setWaitSemaphoreCount(1u)
    		.setPWaitSemaphores(&renderFinishedSemaphore);
    	queue.presentKHR(presentInfo);

    	keepAlive.push_back(cb);

		glfwPollEvents();
    	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
    		glfwSetWindowShouldClose(window, GLFW_TRUE);
    	}
    }

    // Vk cleanup:
	device.freeCommandBuffers(commandPool, commandBuffers);
	device.destroy(commandPool);
	helpers::Free_Memory(device, clearColorMemory);
	device.destroy(clearColorBuffer);
	device.destroy(swapchain);
	helpers::DestroyLogicalDevice(device);
	helpers::DestroySurface(vkInst, surface);
	helpers::DestroyVulkanInstance(vkInst);
	helpers::DestroyWindow(window);
    glfwTerminate();
    return 0;
}

