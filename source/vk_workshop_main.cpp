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
	// ------------------------------------------------------------------------------
	// Task from Part 2: Load 100 images from file, which contain a sprite-animation of an explosion!
	// 
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
	// ------------------------------------------------------------------------------

	// ===> 12. Prepare command buffers for each combination explosion-image and swapchain-image :O
	// ------------------------------------------------------------------------------
	// Task from Part 2: Prepare a REUSABLE command buffer for every single one of the explosion-images to copy it into a given swap chain image!
	// 
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
	// ------------------------------------------------------------------------------

	// ===> 13. Start our render loop and display a wonderful sprite animation:
	const double startTime = glfwGetTime();
    while(!glfwWindowShouldClose(window)) {
    	auto curTime = glfwGetTime();
		// ------------------------------------------------------------------------------
    	// Task from Part 2: Implement the sprite-animation by copying the correct image at the correct time into the correct swap chain image!
    	//
    	static auto lastAniTime = startTime;
    	static auto explosionAniIndex = size_t{0};
    	if (curTime - lastAniTime > 0.016667) {
    		explosionAniIndex = (explosionAniIndex + 1) % 100;
    		lastAniTime = curTime;
    	}
		// ------------------------------------------------------------------------------

    	
    	// Create a semaphore that will be signalled as soon as an image becomes available:
		auto imageAvailableSemaphore = device.createSemaphore(vk::SemaphoreCreateInfo{});
    	// Request the next image (we'll get the index returned, we already have gotten the image handles in ===> 8.):
		auto swapChainImageIndex = device.acquireNextImageKHR(swapchain, std::numeric_limits<uint64_t>::max(), imageAvailableSemaphore, nullptr).value;
    	auto& currentSwapchainImage = swapchainImages[swapChainImageIndex];

    	// Create a semaphore that will be signalled when rendering has finished:
		auto renderFinishedSemaphore = device.createSemaphore(vk::SemaphoreCreateInfo{});

    	// In addition to reusing our command buffers, we should strive to also reuse our semaphores.
    	// There is actually no point in creating new ones every frame. After a semaphore has been signalled, it can be used again.
    	// TODO Part 3: Do not create new semaphores every frame but create them only once and reuse them!
    	//              This applies to both, the imageAvailableSemaphore and the renderFinishedSemaphore
    	
    	// Submit the command buffer
		// ------------------------------------------------------------------------------
    	// Task from Part 2: Submit the command buffer that we have selected!
    	//
    	auto selectedCommandBuffer = preparedCommandBuffers[swapChainImageIndex][explosionAniIndex];
		// ------------------------------------------------------------------------------
    	vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eTransfer; 
    	auto submitInfo = vk::SubmitInfo{}
    		.setCommandBufferCount(1u)
    		.setPCommandBuffers(&selectedCommandBuffer)
    		.setWaitSemaphoreCount(1u)
    		.setPWaitSemaphores(&imageAvailableSemaphore) // <--- (*1) Wait on the swap chain image to become available
    		.setSignalSemaphoreCount(1u)
    		.setPSignalSemaphores(&renderFinishedSemaphore) // Another semaphore: This will be signalled as soon as this batch of work has completed. 
    		.setPWaitDstStageMask(&waitStage);
		queue.submit({ submitInfo }, nullptr); // <--- Hint: the second parameter is yearning for a fence... ;) ;) 
		
    	// Present the image to the screen:
    	auto presentInfo = vk::PresentInfoKHR{} 
    		.setSwapchainCount(1u)              
    		.setPSwapchains(&swapchain)
			.setPImageIndices(&swapChainImageIndex)
    		.setWaitSemaphoreCount(1u)
    		.setPWaitSemaphores(&renderFinishedSemaphore); // Wait until rendering has finished (until vkQueueSubmit has signalled the renderFinishedSemaphore)
    	queue.presentKHR(presentInfo);

    	// The following line of code has probably left a nasty taste ever since you've first encountered it.
    	// this application can not be called a properly behaving real-time rendering application, if we are
    	// waiting for the device to become idle every frame.
    	device.waitIdle(); // TODO Part 3: Remove the waitIdle call and deal with the consequences!
    	                   //              Watch what happens after the sprite animation is starting to repeat (i.e. frame 101 ff.)
    	                   //              (If nothing crashes and the application continues to work normally, take a look at
    	                   //               the console --- at least the validation layers should complain about something.)
    	                   //
    	                   //              Ask yourself the following questions:
    	                   //               - Why did we have the waitIdle call here?
    	                   //               - How much work are we submitting to the GPU and in which pace?
    	                   //               - Are we ever waiting on the GPU? Or put differently: How do we sync host and device?
    	                   //
    	                   //              If you, after serious consideration, come to the conclusion that work between the
    	                   //              host and the device needs to be synchronized, there is particular synchronization
    	                   //              primitive in Vulkan which is made for exactly that: vk::Fence
    	                   //              Read up on fences in the Vulkan specification, if you decide to use them:
    	                   //              https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html#synchronization-fences
    	                   //
    	                   //              One more thing to consider:
    	                   //              At the top, we have assigned CONCURRENT_FRAMES to a value of 3 (if you haven't changed it).
    	                   //              Let's assume that we want to maximize FPS of our very demanding application and work on
    	                   //              #CONCURRENT_FRAMES frames at the same time. What does this mean for using our resources?
    	                   //              How do we prevent that multiple concurrent frames are using the same resources concurrently?
    	                   //              
    	                   //              Note: We have also requested the same amount of swap chain images. It is cool to use the
    	                   //                    same number of concurrent frames and swap chain images. But just to avoid wrong dogmas:
    	                   //                    Those two do NOT have to be the same. It is perfectly fine if the number of concurrent
    	                   //                    frames and the number of swap chain images are set to different values. Of course, there
    	                   //                    will probably be some implicit dependencies between those two, where diverging numbers
    	                   //                    will result in different amounts of parallelism. I hope, I've made this point clear.
    	                   //                    Anyways, in our workshop both are just set to the same value: #CONCURRENT_FRAMES.
    	                   //              
    	device.destroySemaphore(renderFinishedSemaphore);
    	device.destroySemaphore(imageAvailableSemaphore);
    	
		glfwPollEvents();
    	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
    		glfwSetWindowShouldClose(window, GLFW_TRUE);
    	}
    }

	// ===> 14. Perform cleanup
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
