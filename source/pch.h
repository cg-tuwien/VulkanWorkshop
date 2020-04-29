// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H

#define NOMINMAX
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>

#include <array>
#include <vector>
#include <list>
#include <iostream>
#include <functional>

#include <stb_image.h>
#include <tiny_obj_loader.h>

#include "helper_functions.hpp"

#endif //PCH_H
