#include "pch.h"

int main()
{
    glfwInit();

	// Define some constants to be used throughout the program:
	const int WIDTH = 800;  // Leave at 800x800, please!
	const int HEIGHT = 800;
	const uint32_t CONCURRENT_FRAMES = 3; // How many frames do we want our swapchain to have/to render in parallel

	// A storage for resource cleanup handlers (will be invoked at the end of the application):
	std::list<std::function<void()>> cleanupHandlers;
	
	// ===> 1. Create a window
	auto window = helpers::create_window_with_glfw(WIDTH, HEIGHT);
	// ===> 2. Create a vulkan instance
	auto vkInst = helpers::create_vulkan_instance_with_validation_layers();
	// ===> 3. Create a surface to draw into
	auto surface = helpers::create_surface(window, vkInst);
	// ===> 4. Get a handle to (one) physical device, i.e. to the GPU
    auto physicalDevice = vkInst.enumeratePhysicalDevices().front(); // TODO Part 1: If you have multiple GPUs (on a laptop, for instance), select the one you want to use for rendering!
	// ===> 5. Create a logical device which serves as this application's interface to the physical device
	auto device = helpers::create_logical_device(physicalDevice, surface);	
	// ===> 6. Get a queue on the logical device so we can send commands to it
	auto [queueFamilyIndex, queue] = helpers::get_queue_on_logical_device(physicalDevice, surface, device);

	// ===> 7. Create a swapchain
	auto swapchainCreateInfo = vk::SwapchainCreateInfoKHR{}
		.setSurface(surface)
		.setImageExtent(vk::Extent2D{WIDTH, HEIGHT})
		.setMinImageCount(CONCURRENT_FRAMES)
		.setImageArrayLayers(1u)
		.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst)
		.setImageFormat(vk::Format::eB8G8R8A8Unorm)				// TODO Part 1: Query physical device for supported formats and select one!
		.setImageColorSpace(vk::ColorSpaceKHR::eSrgbNonlinear)	//              ... or actually format + color space combinations
		.setPresentMode(vk::PresentModeKHR::eFifo);				// TODO Part 1: Query physical device for supported presentation modes and select one!
	auto swapchain = device.createSwapchainKHR(swapchainCreateInfo);

	// ===> 8. Get all the swapchain's images
	auto swapchainImages = device.getSwapchainImagesKHR(swapchain);

	//*****************
	// Here's plan #1:
	//   We are going to implement manual clearing of our swapchain images (a.k.a. backbuffer images).
	//   For this, we'll fill a 800x800 sized buffer (with 4 color components) and set the chosen clear
	//   color to every pixel.
	//   When we have that, we're going to copy it to the swapchain image.

	// ===> 9. Define memory for a clear color and put it into a buffer
	auto clearColorData = std::make_unique<std::array<std::array<uint8_t, WIDTH * HEIGHT * 4>, CONCURRENT_FRAMES>>();
	for (size_t i = 0; i < WIDTH * HEIGHT * 4; i += 4) {
		// For concurrent frame at index 0:
		static_assert( 0 < CONCURRENT_FRAMES );
		(*clearColorData)[0][i + 0] =   0; // TODO Part 1: Set a clear color of your liking!
		(*clearColorData)[0][i + 1] =   0;
		(*clearColorData)[0][i + 2] = 255;
		(*clearColorData)[0][i + 3] = 255;

		// For concurrent frame at index 1:
		static_assert( 1 < CONCURRENT_FRAMES );
		(*clearColorData)[1][i + 0] =   0; // TODO Part 1: Set a clear color of your liking!
		(*clearColorData)[1][i + 1] = 255;
		(*clearColorData)[1][i + 2] =   0;
		(*clearColorData)[1][i + 3] = 255;

		// For concurrent frame at index 2:
		static_assert( 2 < CONCURRENT_FRAMES );
		(*clearColorData)[2][i + 0] = 255; // TODO Part 1: Set a clear color of your liking!
		(*clearColorData)[2][i + 1] =   0;
		(*clearColorData)[2][i + 2] =   0;
		(*clearColorData)[2][i + 3] = 255;
	}
	// Create the buffer:
	std::array<vk::Buffer, CONCURRENT_FRAMES> clearBuffers;
	for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) {
		// Describe a new buffer:
		auto createInfo = vk::BufferCreateInfo{}
			.setSize(static_cast<vk::DeviceSize>(WIDTH * HEIGHT * 4))
			.setUsage(vk::BufferUsageFlagBits::eTransferSrc);
		clearBuffers[i] = device.createBuffer(createInfo);
		vk::Buffer buffer1;
		// Allocate the memory (we want host-coherent memory):
		auto memory = helpers::allocate_host_coherent_memory_for_given_requirements(physicalDevice, device, createInfo.size, device.getBufferMemoryRequirements(clearBuffers[i]));

		// Bind the buffer handle to the memory:
		device.bindBufferMemory(clearBuffers[i], memory, 0); // TODO Part 1: Use only ONE memory allocation for all buffers, and bind them to different offsets!

		// Copy the colors of your liking into the buffer's memory:
		auto clearColorMappedMemory = device.mapMemory(memory, 0, createInfo.size);
		memcpy(clearColorMappedMemory, (*clearColorData)[i].data(), createInfo.size);
		device.unmapMemory(memory);

		// Make sure to clean up at the end of the application:
		cleanupHandlers.emplace_back([device, buffer=clearBuffers[i], memory](){
			helpers::free_memory(device, memory);
			device.destroyBuffer(buffer);
		});
	}

	// ===> 10. Create a command pool so that we can create commands
	auto commandPoolCreateInfo = vk::CommandPoolCreateInfo{}
		.setQueueFamilyIndex(queueFamilyIndex);
	auto commandPool = device.createCommandPool(commandPoolCreateInfo);
	
	// ===> 11. Start our render loop and clear those swap chain images!!
	const double startTime = glfwGetTime();
    while(!glfwWindowShouldClose(window)) {
    	auto curTime = glfwGetTime();
		
    	// Create a semaphore that will be signalled as soon as an image becomes available:
		auto imageAvailableSemaphore = device.createSemaphore(vk::SemaphoreCreateInfo{});
    	// Request the next image (we'll get the index returned, we already have gotten the image handles in ===> 8.):
		auto swapChainImageIndex = device.acquireNextImageKHR(swapchain, std::numeric_limits<uint64_t>::max(), imageAvailableSemaphore, nullptr).value;
    	auto& currentSwapchainImage = swapchainImages[swapChainImageIndex];

    	// As soon as we have the image, let's copy the clear color into it!
    	// ^ what is meant by "as soon as we have the image" is the following:
    	//   We record the command buffer right now -- which is most likely BEFORE the
    	//   requested swap chain image has been acquired.
    	//   vkAcquireNextImageKHR will signal the imageAvailableSemaphore when is has acquired the image.
    	//   The very same imageAvailableSemaphore is set as a "wait semaphore" to the VkSubmitInfo below. (*1)
    	auto commandBuffer = helpers::allocate_command_buffer(device, commandPool);
    	commandBuffer.begin(vk::CommandBufferBeginInfo().setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
		//
    	// Attention:   The following call (which is vkCmdCopyBufferToImage in disguise) is producing validation errors (see console)
    	// TODO Part 1: Fix those validation errors by adding suitable image layout transitions!
    	//				Feel free to use helpers::establish_pipeline_barrier_with_image_layout_transition
    	//				
		helpers::copy_buffer_to_image(commandBuffer, clearBuffers[swapChainImageIndex], currentSwapchainImage, 800, 800);
    	commandBuffer.end();

    	// Create a semaphore that will be signalled when rendering has finished:
		auto renderFinishedSemaphore = device.createSemaphore(vk::SemaphoreCreateInfo{});
    	// Submit the command buffer
    	vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eAllCommands; // TODO Part 1: Can we wait in a specific/later stage?
    	auto submitInfo = vk::SubmitInfo{}
    		.setCommandBufferCount(1u)
    		.setPCommandBuffers(&commandBuffer)
    		.setWaitSemaphoreCount(1u)
    		.setPWaitSemaphores(&imageAvailableSemaphore) // <--- (*1) Wait on the swap chain image to become available
    		.setSignalSemaphoreCount(1u)
    		.setPSignalSemaphores(&renderFinishedSemaphore) // Another semaphore: This will be signalled as soon as this batch of work has completed. 
    		.setPWaitDstStageMask(&waitStage);
		queue.submit({ submitInfo }, nullptr);
		
    	// Present the image to the screen:
    	                                        // Also the present instruction is producing validation errors because the image is not in the right layout.
    	auto presentInfo = vk::PresentInfoKHR{} // TODO Part 1: Add suitable image layout transitions, s.t. the image is in vk::ImageLayout::ePresentSrcKHR layout!
    		.setSwapchainCount(1u)              //              Think about where the right place would be to add those image layout transitions!
    		.setPSwapchains(&swapchain)
			.setPImageIndices(&swapChainImageIndex)
    		.setWaitSemaphoreCount(1u)
    		.setPWaitSemaphores(&renderFinishedSemaphore); // Wait until rendering has finished (until vkQueueSubmit has signalled the renderFinishedSemaphore)
    	queue.presentKHR(presentInfo);

    	device.waitIdle();
    	device.destroySemaphore(renderFinishedSemaphore);
    	device.freeCommandBuffers(commandPool, 1, &commandBuffer);
    	device.destroySemaphore(imageAvailableSemaphore);
    	
		glfwPollEvents();
    	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
    		glfwSetWindowShouldClose(window, GLFW_TRUE);
    	}
    }

    // Perform cleanup:
	for (auto it = cleanupHandlers.rbegin(); it != cleanupHandlers.rend(); ++it) {
		(*it)();
	}
	device.destroy(commandPool);
	device.destroy(swapchain);
	helpers::destroy_logical_device(device);
	helpers::destroy_surface(vkInst, surface);
	helpers::destroy_vulkan_instance(vkInst);
	helpers::destroy_window(window);
    glfwTerminate();
    return 0;
}
