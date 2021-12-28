#include "Surface.h"
#include "utils.h"

Surface::Surface(std::shared_ptr<Instance> instance, GLFWwindow* window):mInstance(instance)
{
    printVkError(glfwCreateWindowSurface(mInstance->get(), window, nullptr, &mSurface), "create surface");
}

Surface::~Surface()
{
    vkDestroySurfaceKHR(mInstance->get(), mSurface, nullptr);
}

VkSurfaceKHR Surface::get() const
{
    return mSurface;
}
