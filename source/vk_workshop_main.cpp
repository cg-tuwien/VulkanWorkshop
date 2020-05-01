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

			commandBuffer.end();
			preparedCommandBuffers[si][ei] = commandBuffer;

			// Make sure leave the world as the same nice place as it has been before:
			cleanupHandlers.emplace_back([device, commandPool, commandBuffer](){
				helpers::free_command_buffer(device, commandPool, commandBuffer);
			});
		}
	}

	//
	// RENDERPASS, FRAMEBUFFER, GRAPHICS PIPELINE
	// 
	// There can't be a Workshop for a graphics API without actually using a graphics pipeline, can there?
	// Rendering with a graphics pipeline requires quite a bit of setup effort, however.
	// Here we go:
	// 
	
	// ===> 13. Load a 3D model from file and store it into a host coherent buffer
	auto [numberOfVertices, vertexBufferPositions, positionsMemory, vertexBufferTexCoords, texCoordsMemory, vertexBufferNormals, normalsMemory]
		= helpers::load_positions_and_texture_coordinates_and_normals_of_obj("models/hextraction_pod.obj", device, physicalDevice, "tile");
	// Combine in an array for later use when issuing the draw call:
	std::array<vk::Buffer, 2> vertexBuffers{ vertexBufferPositions, vertexBufferTexCoords }; 
	std::array<vk::DeviceSize, 2> vertexBufferOffsets{ 0, 0 };

	auto [textureBuffer, textureBufferMemory, textureWidth, textureHeight] = helpers::load_image_into_host_coherent_buffer(physicalDevice, device, "models/p_pod_diffuse.jpg");
	// ------------------------------------------------------------------------------
	// Task from Part 5: Load the 3D model's texture into a vk::Image that can be used as sampled image in shaders!
	auto [texture, textureMemory] = helpers::create_image(device, physicalDevice, textureWidth, textureHeight, vk::Format::eB8G8R8A8Unorm, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst);
	// transfer it into device memory:
	auto loadIntoTextureCmdBfr = helpers::allocate_command_buffer(device, commandPool);
    loadIntoTextureCmdBfr.begin(vk::CommandBufferBeginInfo{}.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
	helpers::establish_pipeline_barrier_with_image_layout_transition(loadIntoTextureCmdBfr, 
		             vk::PipelineStageFlagBits::eTopOfPipe, /* src -> dst */ vk::PipelineStageFlagBits::eTransfer,
		                                 vk::AccessFlags{}, /* src -> dst */ vk::AccessFlagBits::eTransferWrite,
		texture,               vk::ImageLayout::eUndefined, /* old -> new */ vk::ImageLayout::eTransferDstOptimal
	);

	helpers::copy_buffer_to_image(loadIntoTextureCmdBfr, textureBuffer, texture, textureWidth, textureHeight);
	
	helpers::establish_pipeline_barrier_with_image_layout_transition(loadIntoTextureCmdBfr, 
		                       vk::PipelineStageFlagBits::eTransfer, /* src -> dst */ vk::PipelineStageFlagBits::eBottomOfPipe,
		                         vk::AccessFlagBits::eTransferWrite, /* src -> dst */ vk::AccessFlags{},
		texture,               vk::ImageLayout::eTransferDstOptimal, /* old -> new */ vk::ImageLayout::eShaderReadOnlyOptimal
	);
	loadIntoTextureCmdBfr.end();
	queue.submit({ vk::SubmitInfo{0u, nullptr, nullptr, 1u, &loadIntoTextureCmdBfr, 0u, nullptr} }, nullptr);
	queue.waitIdle(); // We are making our lives easy here

	// Crate a view onto the texture
	auto textureImageView = helpers::create_image_view(device, physicalDevice, texture, vk::Format::eB8G8R8A8Unorm, vk::ImageAspectFlagBits::eColor);
	
	// Furthermore, create an image sampler
	auto samplerCreateInfo = vk::SamplerCreateInfo{}
		.setMinFilter(vk::Filter::eLinear)
		.setMagFilter(vk::Filter::eLinear)
		.setAddressModeU(vk::SamplerAddressMode::eRepeat)
		.setAddressModeV(vk::SamplerAddressMode::eRepeat)
		.setAddressModeW(vk::SamplerAddressMode::eRepeat)
		.setAnisotropyEnable(VK_FALSE)
		.setMaxAnisotropy(1.0f)
		.setBorderColor(vk::BorderColor::eIntOpaqueBlack)
		.setUnnormalizedCoordinates(VK_FALSE)
		.setCompareEnable(VK_FALSE)
		.setCompareOp(vk::CompareOp::eAlways)
		.setMipmapMode(vk::SamplerMipmapMode::eNearest)
		.setMinLod(0.0)
		.setMaxLod(0.0)
		.setMipLodBias(0.0);
	auto sampler = device.createSampler(samplerCreateInfo);
	// ------------------------------------------------------------------------------
	
	// ===> 14. Create an image to be used as depth attachment
	auto [depthImage, depthImageMemory] = helpers::create_image(
		device, physicalDevice,
		WIDTH, HEIGHT, vk::Format::eD32Sfloat, vk::ImageUsageFlagBits::eDepthStencilAttachment
	);

	// ===> 15. Create image views to wrap around all our images: the swap chain images, and the depth image
	// It is common practice in the Vulkan API to refer to image views instead of images directly.
	std::array<vk::ImageView, CONCURRENT_FRAMES> colorImageViews;
	for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) {
		colorImageViews[i] = helpers::create_image_view(device, physicalDevice, swapchainImages[i], selectedFormatAndColorSpace.format, vk::ImageAspectFlagBits::eColor);
	}
	auto depthImageView = helpers::create_image_view(device, physicalDevice, depthImage, vk::Format::eD32Sfloat, vk::ImageAspectFlagBits::eDepth);

	// ===> 16. Create a renderpass
	// A renderpass can only be used with graphics pipelines (not with compute or ray tracing pipelines).
	// Actually, when you want to use a graphics pipeline, you MUST use a renderpass. The renderpass is
	// used to describe a few neccessary things w.r.t. graphics pipelines, which are the following:
	// 
	//  1) It describes which attachments a framebuffer will/must have, that is going to be used with a
	//     graphics pipeline.
	//     
	//  2) It describes what we are doing with the attachments of the bound framebuffer. Are we going to
	//     clear them? are we going to store them? What is each attachment exactly used for (color, depth, ...)?
	//
	//  3) It describes synchronization: Which previous work/memory do we have to wait on before we can
	//     start executing the graphics pipeline? Which work/memory access that comes after a graphics
	//     pipeline must wait on the completion/memory writes of the graphics pipeline, that is represented
	//     by our renderpass? (These are the EXTERNAL SUBPASS DEPENDENCIES)
	//
	//  4) As an additional feature, we can have multiple so called "subpasses" within a renderpass.
	//     They actually describe exactly the same as items 2) and 3), BUT in a different scope: namely
	//     WITHIN the bounds of a renderpass. In terms of resources this also means that memory dependencies
	//     are restricted to the framebuffer's attachments. (These are the INTERNAL SUBPASS DEPENDENCIES)
	//
	//     Note: Multiple subpasses can be useful for things like deferred shading, or whenever a depth-pre-pass
	//           is utilized. In the first subpass, the depth buffer could be rendered, and in a second subpass,
	//           the contents of the depth buffer could be read as input attachment.
	//
	// In our workshop code, we will utilize only one single subpass. That also means that we are only
	// going to define external subpass dependencies, because there are no internal subpass dependencies.

	// ad 1) and partially 2) Create the descriptions of what we are going to do with the framebuffer attachments
	std::array<vk::AttachmentDescription, 2> attachmentDescriptions{
		// Describe the color attachment:
		vk::AttachmentDescription{}
			.setFormat(selectedFormatAndColorSpace.format).setSamples(vk::SampleCountFlagBits::e1)
			.setLoadOp(vk::AttachmentLoadOp::eLoad)			// What do do with the image when the renderpass starts? => We have already stored something in the image => preserve a.k.a. load this data!
			.setStoreOp(vk::AttachmentStoreOp::eStore)		// What to do with the image when the renderpass has finished? => We want the final image be stored, so that it can be presented
			.setInitialLayout(vk::ImageLayout::eTransferDstOptimal)	// When the renderpass starts, in which layout will the image be? => We removed image layout transition after helpers::copy_buffer_to_image, we can handle it here!
			.setFinalLayout(vk::ImageLayout::ePresentSrcKHR),	// When the renderpass finishes, in which layout shall the image be transfered? => The image shall be presented directly afterwards. 
		// Describe the depth attachment:
		vk::AttachmentDescription{}
			.setFormat(vk::Format::eD32Sfloat).setSamples(vk::SampleCountFlagBits::e1)
			.setLoadOp(vk::AttachmentLoadOp::eClear)		// What do do with the image when the renderpass starts? => Make sure that we have cleared the content of previous frames!
			.setStoreOp(vk::AttachmentStoreOp::eDontCare)	// What to do with the image when the renderpass has finished? => We don't need the depth buffer for anything afterwards.
			.setInitialLayout(vk::ImageLayout::eUndefined)	// When the renderpass starts, in which layout will the image be?
			.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal),	// When the renderpass finishes, in which layout shall the image be transferred? => It will be in eDepthStencilAttachmentOptimal layout anyways.
	};

	// ad 2) Describe per subpass for each attachment how it is going to be used, and into which layout it shall be transferred
	auto colorAttachmentInSubpass0 = vk::AttachmentReference{ 0u, vk::ImageLayout::eColorAttachmentOptimal }; // Describes the index (w.r.t. attachmentDescriptions) and the desired layout of the color attachment for subpass 0
	auto depthAttachmentInSubpass0 = vk::AttachmentReference{ 1u, vk::ImageLayout::eDepthStencilAttachmentOptimal }; // Describes the index (w.r.t. attachmentDescriptions) and the desired layout of the depth attachment for subpass 0
	auto subpassDescription = vk::SubpassDescription{}
		.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics) // Despite looking as if it was configurable, only eGraphics is valid/supported
		.setColorAttachmentCount(1u)
		.setPColorAttachments(&colorAttachmentInSubpass0)
		.setPDepthStencilAttachment(&depthAttachmentInSubpass0);

	// ad 3) Describe execution and memory dependencies (just in the same way as with pipeline barriers).
	//        In this case, we only have external dependencies: One with whatever comes before we are using
	//        this renderpass, and another with whatever comes after this renderpass in a queue.
	std::array<vk::SubpassDependency, 2> subpassDependencies{
		vk::SubpassDependency{}
			// Establish proper dependencies with whatever comes before (which is the imageAvailableSemaphore wait and then the command buffer that copies an explosion image to the swapchain image):
			                    .setSrcSubpass(VK_SUBPASS_EXTERNAL) /* -> */ .setDstSubpass(0u)
			//     We have to wait for the transfer of helpers::copy_buffer_to_image to complete before we may render into the image.
			//     We do not need to wait before the eColorAttachmentOutput stage, because only then we are going to write into the image.
			 .setSrcStageMask(vk::PipelineStageFlagBits::eTransfer) /* -> */ .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
			//     The memory written helpers::copy_buffer_to_image is a eTransferWrite => make this memory available.
			//     This memory must be visible to whatever cache/unit/ROP performs eColorAttachmentWrite. We are not going to read from it, therefore eColorAttachmentWrite is the only neccessary access type to include.
			  .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite) /* -> */ .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
		,
		vk::SubpassDependency{}
			// Establish proper dependencies with whatever comes after (which is the renderFinishedSemaphore signal):
			                                                 .setSrcSubpass(0u) /* -> */ .setDstSubpass(VK_SUBPASS_EXTERNAL)
			//     Execution may continue as soon as the eColorAttachmentOutput stage is done.
			//     However, nothing must really wait on that stage, because afterwards comes the semaphore. Hence, eBottomOfPipe.
			.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput) /* -> */ .setDstStageMask(vk::PipelineStageFlagBits::eBottomOfPipe)
			//     The graphics pipeline is performing eColorAttachmentWrites. These need to be made available.
			//     We don't have to make them visible to anything, because the semaphore performs a full memory barrier anyways. 
			       .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite) /* -> */ .setDstAccessMask(vk::AccessFlags{})
	};

	auto renderpassCreateInfo = vk::RenderPassCreateInfo{}
		.setAttachmentCount(2u) // attachmentCount and pAttachments describe 1) and a part of 2)
		.setPAttachments(attachmentDescriptions.data())
		.setSubpassCount(1u) // subpassCount and pSubpasses describe a part of 2)
		.setPSubpasses(&subpassDescription)
		.setDependencyCount(2u) // dependencyCount and pDependencies describe 3)
		.setPDependencies(subpassDependencies.data());
	auto renderpass = device.createRenderPass(renderpassCreateInfo);

	// ===> 17. Create framebuffers, one for each swap chain image, using the IMAGE VIEWS created above, AND the RENDERPASS to describe the usages and layouts
	std::array<vk::Framebuffer, CONCURRENT_FRAMES> framebuffers;
	for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) {
		std::array<vk::ImageView, 2> attachments{ colorImageViews[i], depthImageView };
		auto createInfo = vk::FramebufferCreateInfo{}
			.setAttachmentCount(2u) // Attachments a.k.a. image views
			.setPAttachments(attachments.data())
			.setRenderPass(renderpass) // Renderpass
			.setWidth(WIDTH).setHeight(HEIGHT).setLayers(1u);
		framebuffers[i] = device.createFramebuffer(createInfo);
	}
	
	// Whew, that was already a lot to describe, but the biggest chunk is yet to come:
	// ===> 18. Create the graphics pipeline, describe every state of it
	auto [vertShader, vertStageInfo] = helpers::load_shader_and_create_shader_module_and_stage_info(device, "shaders/vertex_shader.spv", vk::ShaderStageFlagBits::eVertex);
	auto [fragShader, fragStageInfo] = helpers::load_shader_and_create_shader_module_and_stage_info(device, "shaders/fragment_shader.spv", vk::ShaderStageFlagBits::eFragment);
	// Describe the shaders used:
	std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages{ vertStageInfo, fragStageInfo };
	// Describe the vertex input, i.e. two vertex input attributes in our case:
	std::array<vk::VertexInputBindingDescription, 2> vertexBindings{
		vk::VertexInputBindingDescription{}.setBinding(0u).setStride(sizeof(glm::vec3)).setInputRate(vk::VertexInputRate::eVertex), // positions of the 3D model as vertex input
		vk::VertexInputBindingDescription{}.setBinding(1u).setStride(sizeof(glm::vec2)).setInputRate(vk::VertexInputRate::eVertex)  // texture coordinates of the 3D model as vertex input
	};
	std::array<vk::VertexInputAttributeDescription, 2> attributeDescriptions{
		vk::VertexInputAttributeDescription{}.setBinding(0u).setLocation(0u).setFormat(vk::Format::eR32G32B32Sfloat).setOffset(0u), // describe the format of the positions and the input-location in the shader
		vk::VertexInputAttributeDescription{}.setBinding(1u).setLocation(1u).setFormat(vk::Format::eR32G32Sfloat).setOffset(0u)     // describe the format of the texture coordinates and the input-location in the shader
	};
	auto vertexInputState = vk::PipelineVertexInputStateCreateInfo{}
		.setVertexBindingDescriptionCount(2u).setPVertexBindingDescriptions(vertexBindings.data())
		.setVertexAttributeDescriptionCount(2u).setPVertexAttributeDescriptions(attributeDescriptions.data());
	// Describe the topology of the vertices
	auto inputAssemblyState = vk::PipelineInputAssemblyStateCreateInfo{}.setTopology(vk::PrimitiveTopology::eTriangleList);
	// Describe viewport and scissors state
	auto viewport = vk::Viewport{}.setX(0.0f).setY(0.0f).setWidth(WIDTH).setHeight(HEIGHT).setMinDepth(0.0f).setMaxDepth(1.0f);
	auto scissors = vk::Rect2D{}.setOffset({0, 0}).setExtent({WIDTH, HEIGHT});
	auto viewportState = vk::PipelineViewportStateCreateInfo{}
		.setViewportCount(1u).setPViewports(&viewport)
		.setScissorCount(1u).setPScissors(&scissors);
	// Describe the rasterizer state
	auto rasterizerState = vk::PipelineRasterizationStateCreateInfo{}
		.setPolygonMode(vk::PolygonMode::eFill)
		.setLineWidth(1.0f) // reasons...
		.setCullMode(vk::CullModeFlagBits::eBack)
		.setFrontFace(vk::FrontFace::eCounterClockwise);
	// Describe multisampling state
	auto multisampleState = vk::PipelineMultisampleStateCreateInfo{}.setRasterizationSamples(vk::SampleCountFlagBits::e1);
	// Configure depth/stencil state
	auto depthStencilState = vk::PipelineDepthStencilStateCreateInfo{}
		.setDepthTestEnable(VK_TRUE)
		.setDepthWriteEnable(VK_TRUE)
		.setDepthCompareOp(vk::CompareOp::eLess);
	// Configure blending and which color channels are written
	auto colorBlendAttachmentState = vk::PipelineColorBlendAttachmentState{}
		.setBlendEnable(VK_FALSE)
		.setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA); // write all color components
	auto colorBlendState = vk::PipelineColorBlendStateCreateInfo{}.setAttachmentCount(1u).setPAttachments(&colorBlendAttachmentState);

	// NOT SO FAST MY FRIEND... we have to take a little detour and create some GPU resources and DESCRIPTORS which we can pass to shaders
	// ===> 19. Create resource descriptors on the GPU
	// First, let's create uniform buffers to be used concurrently and in shaders:
	// ------------------------------------------------------------------------------
	// Task from Part 5: Make a separate uniformBuffer for every concurrent frame and fix real/potential animation stuttering this way!
	std::array<std::tuple<vk::Buffer, vk::DeviceMemory>, CONCURRENT_FRAMES> ubos;
	for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) {
		ubos[i] = helpers::create_host_coherent_buffer_and_memory(
			device, physicalDevice, 
			sizeof(std::array<glm::mat4, 3>), 
			vk::BufferUsageFlagBits::eUniformBuffer
		);
	}

	// In order to make it available to shaders, we have to create a DESCRIPTOR for it.
	// However, in order to create a descriptor, we have to create a DESCRIPTOR POOL first:
	std::array<vk::DescriptorPoolSize, 2> poolSizes{ // Descriptors are also allocated from pools---make sure that the pool is large enough for our requirements.
		vk::DescriptorPoolSize{}.setType(vk::DescriptorType::eUniformBuffer).setDescriptorCount(1u),
		// ------------------------------------------------------------------------------
		// Task from Part 5: In order to create the additional eCombinedImageSamplers, we have to extend our pool size
		vk::DescriptorPoolSize{}.setType(vk::DescriptorType::eCombinedImageSampler).setDescriptorCount(1u)
		// ------------------------------------------------------------------------------
	};
	auto descriptorPoolCreateInfo = vk::DescriptorPoolCreateInfo{}
		.setMaxSets(CONCURRENT_FRAMES)
		.setPoolSizeCount(static_cast<uint32_t>(poolSizes.size()))
		.setPPoolSizes(poolSizes.data());
	auto descriptorPool = device.createDescriptorPool(descriptorPoolCreateInfo);

	// With the descriptor pool in place, let's create a descriptor for our uniform buffer:
	//
	// But again: not so fast! We have to define the LAYOUT of our descriptors first
	std::array<vk::DescriptorSetLayoutBinding, 2> layoutBindings{
		vk::DescriptorSetLayoutBinding{ 0u, vk::DescriptorType::eUniformBuffer, 1u, vk::ShaderStageFlagBits::eVertex },
		// ------------------------------------------------------------------------------
		// Task from Part 5: The new descriptor has to be added to the pipeline layout, we are using "layout(set = 0, binding = 1)" in shaders => binding 1 in this descriptor set
		vk::DescriptorSetLayoutBinding{ 1u, vk::DescriptorType::eCombinedImageSampler, 1u, vk::ShaderStageFlagBits::eFragment }
		// ------------------------------------------------------------------------------
	};
	auto descriptorSetLayout = device.createDescriptorSetLayout(
		vk::DescriptorSetLayoutCreateInfo{}
			.setBindingCount(static_cast<uint32_t>(layoutBindings.size()))
			.setPBindings(layoutBindings.data())
	);

	// Okay... we've got the descriptor set LAYOUT. Now, let's create ACTUAL DESCRIPTOR SETS (containing actual resource descriptors):
	std::array<vk::DescriptorSetLayout, CONCURRENT_FRAMES> sameLayouts;
	for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) { sameLayouts[i] = descriptorSetLayout; }
	auto descriptorSets = device.allocateDescriptorSets(
		vk::DescriptorSetAllocateInfo{}
			.setDescriptorPool(descriptorPool)
			.setDescriptorSetCount(CONCURRENT_FRAMES)
			.setPSetLayouts(sameLayouts.data())
	);
	assert (CONCURRENT_FRAMES == descriptorSets.size()); // We have requested #CONCURRENT_FRAMES descriptor set => have we gotten #CONCURRENT_FRAMES?

	// The descriptor set(s) are allocated, but they contain no data so far
	//  => let's fill them with some useful data, a.k.a. WRITE into the DESCRIPTORS
	for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) {
		auto uniformBufferInfo = vk::DescriptorBufferInfo{}.setBuffer(std::get<vk::Buffer>(ubos[i])).setOffset(0).setRange(sizeof(std::array<glm::mat4, 3>));
		// ------------------------------------------------------------------------------
		auto imageInfo = vk::DescriptorImageInfo{}.setImageView(textureImageView).setSampler(sampler).setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
		// ------------------------------------------------------------------------------
		std::array<vk::WriteDescriptorSet, 2> writes{
			vk::WriteDescriptorSet{}
				.setDstSet(descriptorSets[i])
				.setDstBinding(0u)
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(1u).setPBufferInfo(&uniformBufferInfo),
			// ------------------------------------------------------------------------------
			// Task from Part 5: In order to make the texture ("models/p_pod_diffuse.jpg" for the 3D model) accessible, you'll have to create a resource descriptor for it, too!
			vk::WriteDescriptorSet{}
				.setDstSet(descriptorSets[i])
				.setDstBinding(1u)
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setDescriptorCount(1u).setPImageInfo(&imageInfo)
			// ------------------------------------------------------------------------------
		};
		// And perform the actual write:
		device.updateDescriptorSets(static_cast<uint32_t>(writes.size()), writes.data(), 0u, nullptr);
	}
	// ------------------------------------------------------------------------------
	// It's done. Descriptors are written to the GPU. We can use them now in shaders.

	// After a thousand years... 
	// ===> 20. Continue with configuring our graphics pipeline:	
	// Create a PIPELINE LAYOUT which describes all RESOURCES that are passed in to our pipeline (Resource Descriptors that we have created above)
	auto pipelineLayout = device.createPipelineLayout(
		vk::PipelineLayoutCreateInfo{} // A pipeline's layout describes all resources used by a pipeline or in shaders.
			.setSetLayoutCount(1u)
			.setPSetLayouts(&descriptorSetLayout) // We don't need the actual descriptors when defining the PIPELINE. The LAYOUT is sufficient at this point.
	);

	// Put everything together:
	auto pipelineCreateInfo = vk::GraphicsPipelineCreateInfo{}
		.setStageCount(2u).setPStages(shaderStages.data()) 
		.setPVertexInputState(&vertexInputState)
		.setPInputAssemblyState(&inputAssemblyState)
		.setPViewportState(&viewportState)
		.setPRasterizationState(&rasterizerState)
		.setPMultisampleState(&multisampleState)
		.setPDepthStencilState(&depthStencilState)
		.setPColorBlendState(&colorBlendState)
		.setLayout(pipelineLayout)
		.setRenderPass(renderpass).setSubpass(0u); // <--- Which subpass of the given renderpass we are going to use this graphics pipeline for
	// FINALLY:
	auto graphicsPipeline = device.createGraphicsPipeline(nullptr, pipelineCreateInfo);

	// don't need them anymore (the shader code has already been added to the pipeline):
	helpers::destroy_shader_module(device, fragShader);
	helpers::destroy_shader_module(device, vertShader);

	// ===> 21. Create command buffers and record drawing the loaded model using the graphics pipeline.
	//          Again, we will need as many as there are #CONCURRENT_FRAMES frames in flight.
	std::array<vk::CommandBuffer, CONCURRENT_FRAMES> graphicsDrawCommandBuffers;
	for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) {
		auto commandBuffer = helpers::allocate_command_buffer(device, commandPool);
    	commandBuffer.begin(vk::CommandBufferBeginInfo{});

		// Record rendering of the 3D model into commandBuffer using graphicsPipeline
		std::array<vk::ClearValue, 2> clearValues {
			vk::ClearValue{ vk::ClearColorValue{ std::array<float, 4>{1.0f, 0.0f, 0.5f, 1.0f} } },
			vk::ClearValue{ vk::ClearDepthStencilValue{ 1.0f, 0u }}
		};
		auto renderPassBeginInfo = vk::RenderPassBeginInfo{}
			.setRenderPass(renderpass)
			.setFramebuffer(framebuffers[i])
			.setRenderArea({{0, 0}, {WIDTH, HEIGHT}})
			.setClearValueCount(2u).setPClearValues(clearValues.data());
		commandBuffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);

		commandBuffer.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics, pipelineLayout, 
			0u, 1u, &descriptorSets[i], // <--- Bind the actual descriptors to the pipeline
			0u, nullptr
		);

		commandBuffer.bindVertexBuffers(0u, 2u, vertexBuffers.data(), vertexBufferOffsets.data());
		commandBuffer.draw(numberOfVertices, 1u, 0u, 0u);
		
		commandBuffer.endRenderPass();

		commandBuffer.end();
		graphicsDrawCommandBuffers[i] = commandBuffer;
	}

	// ===> 22. Create image available semaphores, render finished semaphores, and fences, one of each PER FRAME IN FLIGHT:
   	std::array<vk::Semaphore, CONCURRENT_FRAMES> imageAvailableSemaphores;
	std::array<vk::Semaphore, CONCURRENT_FRAMES> renderFinishedSemaphores;
	std::array<vk::Fence, CONCURRENT_FRAMES> syncHostWithDeviceFence;
	for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) {
		imageAvailableSemaphores[i] = device.createSemaphore(vk::SemaphoreCreateInfo{});
		renderFinishedSemaphores[i] = device.createSemaphore(vk::SemaphoreCreateInfo{});
		syncHostWithDeviceFence[i] = device.createFence(vk::FenceCreateInfo{}.setFlags(vk::FenceCreateFlagBits::eSignaled));
	}

	//
	// Wouldn't it be the culmination of this little application if the explosion was reflected by the space ship?
	// TODO Part 6: Make the space ship reflect the explosion in the background!
	//              There are several things to consider:
	//               - First of all, the background images aren't vk::Images yet. They are just "stupid" buffers 
	//                 so far. How do you tranfer their contents into textures that can be sampled in shaders?
	//               - Do you make only one of the explosion-textures(?) available to shaders or all 100 of them?
	//               - How do you ensure proper handling of concurrent frames?
	//               - For the reflection itself, you could just keep it simple: Assume a fixed viewer position
	//                 in shaders. You can also implement a more complicated approach and pass the position via
	//                 UBO. It's up to you.
	//               - "But," I hear you say "for reflections we do not have normals passed to shaders yet?!" ... :O !!
	//
	// Good luck with the last task of this workshop!
	// I hope it proved to be a valuable Vulkan learning resource to you.
	// Please send your feedback to junt@cg.tuwien.ac.at and if you would like to contribute or report issues, so so on GitHub!
	//
	// Now go and implement those reflectons! Make it so.
	// 
	
	// ===> 23. Start our render loop and display a wonderful sprite animation, reuse semaphores and embrace multiple frampues in flight:
	const double startTime = glfwGetTime();
    while(!glfwWindowShouldClose(window)) {
    	auto curTime = glfwGetTime();
    	static auto lastAniTime = startTime;
    	static auto explosionAniIndex = size_t{0};
    	const auto animationHz = double{0.033333};
    	if (curTime - lastAniTime > animationHz) {
    		explosionAniIndex = (explosionAniIndex + 1) % 100;
    		lastAniTime = curTime;
    	}

    	// We have to make sure that not more than #CONCURRENT_FRAMES are in flight at
    	// the same time. We can use fences to ensure that. 
    	static auto frameInFlightIndex = int64_t{-1};
    	frameInFlightIndex = (frameInFlightIndex + 1) % CONCURRENT_FRAMES;

    	device.waitForFences(1u, &syncHostWithDeviceFence[frameInFlightIndex], VK_TRUE, std::numeric_limits<uint64_t>::max());
    	device.resetFences(1u, &syncHostWithDeviceFence[frameInFlightIndex]);

    	// Before submitting work to the queue, update data in our host coherent buffer(s):
    	auto easingFunction = [](float n) { n = 1.0f - n; return 1.0f - n*n; };
    	float modelTranslationZ = glm::mix(-5.0f, 10.0f, easingFunction(static_cast<float>(explosionAniIndex) / 100.0f + (curTime - lastAniTime) / (100.0f * animationHz)));
    	auto model = glm::translate(glm::mat4{1.0f}, glm::vec3{0.0f, 0.0f, modelTranslationZ}) * glm::translate(glm::mat4{1.0f}, glm::vec3{2.0f, -2.5f, 0.0f}) * glm::rotate(glm::mat4{1.0f}, glm::radians(0.f), glm::vec3(0.0f, 1.0f, 0.0f)) * glm::scale(glm::mat4{1.0f}, glm::vec3{0.002f});
        auto view =  glm::inverse(glm::mat4{glm::vec4{1,0,0,0}, glm::vec4{0,-1,0,0}, glm::vec4{0,0,1,0}, glm::vec4{0,0,10,1}});
        auto proj = glm::perspective(glm::radians(90.0f), static_cast<float>(WIDTH)/static_cast<float>(HEIGHT), 0.1f, 20.0f);
		std::array<glm::mat4, 3> matrices{ model, view, proj };
		helpers::copy_data_into_host_coherent_memory(device, sizeof(matrices), matrices.data(), std::get<vk::DeviceMemory>(ubos[frameInFlightIndex]));
    	
    	// Request the next image (we'll get the index returned, we already have gotten the image handles in ===> 8.):
		auto swapChainImageIndex = device.acquireNextImageKHR(swapchain, std::numeric_limits<uint64_t>::max(), imageAvailableSemaphores[frameInFlightIndex], nullptr).value;
    	auto& currentSwapchainImage = swapchainImages[swapChainImageIndex];

    	// Submit the command buffer
    	std::array<vk::CommandBuffer, 2> selectedCommandBuffers{
    		// 1st command buffer: Select the right command buffer for clearing the swap chain image
    		preparedCommandBuffers[swapChainImageIndex][explosionAniIndex], 
    		// Note: Selecting it based on the swapChainImageIndex and not based on the frameInFlightIndex
    		//       is not reeeaally bullet-proof. It actually assumes that frameInFlightIndex and
    		//       swapChainImageIndex change in lockstep. This is a bit a dangerous assumption. It does
    		//       not always have to be true. Actually, to be 100% secure, one would have to prepare the
    		//       command buffers for each swapChainImageIndex and for each frameInFlightIndex.
    		//       Just saying.
    		//       This probably also depends on the presentation engine and the concrete presentation
    		//       mode that is being used. If you are interested in investigating this further, you
    		//       are highly encouraged to play around with the parameters.
    		//       
    		// TODO Part 6 (optional/bonus-task): Read the above ^ note and think about solution strategies and/or develop them!
    		//       Of course, we could prepare 900 command buffers... but is that a good idea?
    		//       Does this problem even exist with all swap chain presentation modes? It would
    		//       be a good idea to start researching about presentation modes, analyze their
    		//       characteristics, and based on that knowledge, come up with solutions.

    		// 2nd command buffer: Draw our model with the recorded commands buffers that use our graphics pipeline
    		graphicsDrawCommandBuffers[frameInFlightIndex]
		};
    	vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eTransfer; 
    	auto submitInfo = vk::SubmitInfo{}
    		.setCommandBufferCount(static_cast<uint32_t>(selectedCommandBuffers.size()))
    		.setPCommandBuffers(selectedCommandBuffers.data())
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

		glfwPollEvents();
    	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
    		glfwSetWindowShouldClose(window, GLFW_TRUE);
    	}
    }

	// ===> 24. Perform cleanup
	device.waitIdle(); // Wait idle before destroying anything that might still be in use currently
	device.destroyDescriptorPool(descriptorPool);
	for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) { device.freeCommandBuffers(commandPool, graphicsDrawCommandBuffers[i]); }
	device.destroyPipeline(graphicsPipeline);
	device.destroyPipelineLayout(pipelineLayout);
	device.destroyDescriptorSetLayout(descriptorSetLayout);
	for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) {
		helpers::destroy_buffer(device, std::get<vk::Buffer>(ubos[i]));
		helpers::free_memory(device, std::get<vk::DeviceMemory>(ubos[i]));
	}
	device.destroyRenderPass(renderpass);
	for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) { device.destroyFramebuffer(framebuffers[i]); }
	helpers::destroy_image_view(device, depthImageView);
	for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) { helpers::destroy_image_view(device, colorImageViews[i]); }
	helpers::free_memory(device, depthImageMemory);
	helpers::destroy_image(device, depthImage);
	device.destroySampler(sampler);
	helpers::destroy_image_view(device, textureImageView);
	helpers::free_memory(device, textureMemory);
	helpers::destroy_image(device, texture);
	helpers::free_memory(device, textureBufferMemory);
	helpers::destroy_buffer(device, textureBuffer);
	helpers::free_memory(device, texCoordsMemory);
	helpers::destroy_buffer(device, vertexBufferNormals);
	helpers::free_memory(device, normalsMemory);
	helpers::destroy_buffer(device, vertexBufferTexCoords);
	helpers::free_memory(device, positionsMemory);
	helpers::destroy_buffer(device, vertexBufferPositions);
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
