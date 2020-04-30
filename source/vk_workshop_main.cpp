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
    auto physicalDevice = vkInst.enumeratePhysicalDevices().front(); 
	std::cout << "selected physical device: " << physicalDevice.getProperties().deviceName << std::endl;
	// ===> 5. Create a logical device which serves as this application's interface to the physical device
	auto device = helpers::create_logical_device(physicalDevice, surface);	
	// ===> 6. Get a queue on the logical device so we can send commands to it
	auto [queueFamilyIndex, queue] = helpers::get_queue_on_logical_device(physicalDevice, surface, device);

	auto availableFormats = physicalDevice.getSurfaceFormatsKHR(surface);
	auto availablePresentationModes = physicalDevice.getSurfacePresentModesKHR(surface);
	auto selectedFormatAndColorSpace = availableFormats[0];
	std::cout << "selected swap chain format and color space: " << to_string(selectedFormatAndColorSpace.format) << " " << to_string(selectedFormatAndColorSpace.colorSpace) << std::endl;
	auto selectedPresentationMode = availablePresentationModes[0];
	std::cout << "selected presentation mode: " << to_string(selectedPresentationMode) << std::endl;
	
	// ===> 7. Create a swapchain
	auto swapchainCreateInfo = vk::SwapchainCreateInfoKHR{}
		.setSurface(surface)
		.setImageExtent(vk::Extent2D{WIDTH, HEIGHT})
		.setMinImageCount(CONCURRENT_FRAMES)
		.setImageArrayLayers(1u)
		.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst)
		.setImageFormat(selectedFormatAndColorSpace.format)				
		.setImageColorSpace(selectedFormatAndColorSpace.colorSpace)
		.setPresentMode(selectedPresentationMode);				
	auto swapchain = device.createSwapchainKHR(swapchainCreateInfo);

	// ===> 8. Get all the swapchain's images
	auto swapchainImages = device.getSwapchainImagesKHR(swapchain);

	// ===> 9. Bye bye, clear color buffers, nice to have known you.
	
	// ===> 10. Create a command pool so that we can create commands
	auto commandPoolCreateInfo = vk::CommandPoolCreateInfo{}
		.setQueueFamilyIndex(queueFamilyIndex);
	auto commandPool = device.createCommandPool(commandPoolCreateInfo);

	// ===> 11. Load 100 explosion images from files
	std::array<std::tuple<vk::Buffer, vk::DeviceMemory, int, int>, 100> explosionImages;
	for (size_t i = 1; i <= 100; ++i) {
		auto iToS = "00" + std::to_string(i);
		explosionImages[i-1] = helpers::load_image_into_host_coherent_buffer(
			physicalDevice, device, 
			"images/explosion02HD-frame" + iToS.substr(iToS.length() - 3) + ".tga"
		);
		
		// Make sure to clean up after ourselves:
		cleanupHandlers.emplace_back([device, tpl = explosionImages[i-1]](){
			helpers::free_memory(device, std::get<vk::DeviceMemory>(tpl));
			helpers::destroy_buffer(device, std::get<vk::Buffer>(tpl));
		});
	}

	// ===> 12. Prepare command buffers for each combination explosion-image and swapchain-image :O
	std::array<std::array<vk::CommandBuffer, 100>, CONCURRENT_FRAMES> preparedCommandBuffers;
	for (size_t si = 0; si < CONCURRENT_FRAMES; ++si) {
		assert(swapchainImages.size() == CONCURRENT_FRAMES);
		for (size_t ei = 0; ei < 100; ++ei) {
    		auto commandBuffer = helpers::allocate_command_buffer(device, commandPool);
    		commandBuffer.begin(vk::CommandBufferBeginInfo{});

			helpers::establish_pipeline_barrier_with_image_layout_transition(commandBuffer, 
				             vk::PipelineStageFlagBits::eTopOfPipe, /* src -> dst */ vk::PipelineStageFlagBits::eTransfer,
				                                 vk::AccessFlags{}, /* src -> dst */ vk::AccessFlagBits::eTransferWrite,
				swapchainImages[si],   vk::ImageLayout::eUndefined, /* old -> new */ vk::ImageLayout::eTransferDstOptimal
			);

			helpers::copy_buffer_to_image(commandBuffer, std::get<vk::Buffer>(explosionImages[ei]), swapchainImages[si], 800, 800);
			
			helpers::establish_pipeline_barrier_with_image_layout_transition(commandBuffer, 
				                       vk::PipelineStageFlagBits::eTransfer, /* src -> dst */ vk::PipelineStageFlagBits::eBottomOfPipe,
				                         vk::AccessFlagBits::eTransferWrite, /* src -> dst */ vk::AccessFlags{},
				swapchainImages[si],   vk::ImageLayout::eTransferDstOptimal, /* old -> new */ vk::ImageLayout::ePresentSrcKHR
			);
			
			commandBuffer.end();
			preparedCommandBuffers[si][ei] = commandBuffer;

			// Make sure leave the world as the same nice place as it has been before:
			cleanupHandlers.emplace_back([device, commandPool, commandBuffer](){
				helpers::free_command_buffer(device, commandPool, commandBuffer);
			});
		}
	}

	// ===> 13. Load a 3D model from file and store it into a host coherent buffer
	auto [vertexBufferPositions, vertexBufferTexCoords] = helpers::load_positions_and_texture_coordinates_of_obj("models/hextraction_pod.obj", device, physicalDevice, "tile");
	// TODO: next steps:
	//  - create depth image
	//  - create image views
	//  - create framebuffer
	//  - create renderpass
	//  - create graphics pipeline
	//  - rendurrrr

	// ===> 14. Create image available semaphores, render finished semaphores, and fences, one of each PER FRAME IN FLIGHT:
	// ------------------------------------------------------------------------------
   	// Task from Part 3: Do not create new semaphores every frame but create them only once and reuse them!
   	// 
   	// Okay, why do we need multiple semaphores and even fences now?
   	// Because we have removed the waitIdle call, we are never synchronizing the host with the device.
   	// Our application should be able to work on #CONCURRENT_FRAMES frames at the same time, however.
   	// Therefore -- if there are #CONCURRENT_FRAMES frames at any given time -- we have to have the
   	// same amount of concurrently usable resources at any given time.
   	// In order to synchronize the host with the device (i.e. wait on the CPU if the GPU is running
   	// ahead by more than #CONCURRENT_FRAMES frames), we can wait using fences.
   	std::array<vk::Semaphore, CONCURRENT_FRAMES> imageAvailableSemaphores;
	std::array<vk::Semaphore, CONCURRENT_FRAMES> renderFinishedSemaphores;
	std::array<vk::Fence, CONCURRENT_FRAMES> syncHostWithDeviceFence;
	for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) {
		imageAvailableSemaphores[i] = device.createSemaphore(vk::SemaphoreCreateInfo{});
		renderFinishedSemaphores[i] = device.createSemaphore(vk::SemaphoreCreateInfo{});
		syncHostWithDeviceFence[i] = device.createFence(vk::FenceCreateInfo{}.setFlags(vk::FenceCreateFlagBits::eSignaled));
	}
	// ------------------------------------------------------------------------------

	// ===> 15. Start our render loop and display a wonderful sprite animation, reuse semaphores and embrace multiple frames in flight:
	const double startTime = glfwGetTime();
    while(!glfwWindowShouldClose(window)) {
    	auto curTime = glfwGetTime();
    	static auto lastAniTime = startTime;
    	static auto explosionAniIndex = size_t{0};
    	if (curTime - lastAniTime > 0.016667) {
    		explosionAniIndex = (explosionAniIndex + 1) % 100;
    		lastAniTime = curTime;
    	}

		// ------------------------------------------------------------------------------
    	// We have to make sure that not more than #CONCURRENT_FRAMES are in flight at
    	// the same time. We can use fences to ensure that. 
    	// 
		static auto frameInFlightIndex = size_t{0};
    	frameInFlightIndex = (frameInFlightIndex + 1) % CONCURRENT_FRAMES;

    	device.waitForFences(1u, &syncHostWithDeviceFence[frameInFlightIndex], VK_TRUE, std::numeric_limits<uint64_t>::max());
    	device.resetFences(1u, &syncHostWithDeviceFence[frameInFlightIndex]);
		// ------------------------------------------------------------------------------
    	
    	// Request the next image (we'll get the index returned, we already have gotten the image handles in ===> 8.):
		auto swapChainImageIndex = device.acquireNextImageKHR(swapchain, std::numeric_limits<uint64_t>::max(), imageAvailableSemaphores[frameInFlightIndex], nullptr).value;
    	auto& currentSwapchainImage = swapchainImages[swapChainImageIndex];

    	// Submit the command buffer
    	auto selectedCommandBuffer = preparedCommandBuffers[swapChainImageIndex][explosionAniIndex];
    	vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eTransfer; 
    	auto submitInfo = vk::SubmitInfo{}
    		.setCommandBufferCount(1u)
    		.setPCommandBuffers(&selectedCommandBuffer)
    		.setWaitSemaphoreCount(1u)
    		.setPWaitSemaphores(&imageAvailableSemaphores[frameInFlightIndex]) // <--- (*1) Wait on the swap chain image to become available
    		.setSignalSemaphoreCount(1u)
    		.setPSignalSemaphores(&renderFinishedSemaphores[frameInFlightIndex]) // Another semaphore: This will be signalled as soon as this batch of work has completed. 
    		.setPWaitDstStageMask(&waitStage);
		queue.submit({ submitInfo }, syncHostWithDeviceFence[frameInFlightIndex]);
		
    	// Present the image to the screen:
    	auto presentInfo = vk::PresentInfoKHR{} 
    		.setSwapchainCount(1u)              
    		.setPSwapchains(&swapchain)
			.setPImageIndices(&swapChainImageIndex)
    		.setWaitSemaphoreCount(1u)
    		.setPWaitSemaphores(&renderFinishedSemaphores[frameInFlightIndex]); // Wait until rendering has finished (until vkQueueSubmit has signalled the renderFinishedSemaphore)
    	queue.presentKHR(presentInfo);

		// ------------------------------------------------------------------------------
    	// Task from Part 3: Remove the waitIdle call and deal with the consequences!
    	//  => it's gone, and has caused huge troubles.
    	//     We have submitted endless amounts of work, because we never waited on the
    	//     GPU to finish anything. We just submitted, and submitted, and submitted, etc.
    	//     We have to ensure that the CPU and the GPU stay in sync w.r.t. their
    	//     workloads.
    	//     We have decided that we are going with CONCURRENT_FRAMES-times concurrent
    	//     frames. So, when we produce frame number (X + CONCURRENT_FRAMES), we have
    	//     to make sure that frame (X) has fully completed so that we can reuse
    	//     its resources.
		// ------------------------------------------------------------------------------
    	
		glfwPollEvents();
    	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
    		glfwSetWindowShouldClose(window, GLFW_TRUE);
    	}
    }

	// ===> 14. Perform cleanup
	device.waitIdle(); // Wait idle before destroying anything that might still be in use currently
	for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) {
		device.destroyFence(syncHostWithDeviceFence[i]);
		device.destroySemaphore(renderFinishedSemaphores[i]);
		device.destroySemaphore(imageAvailableSemaphores[i]);
	}
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
