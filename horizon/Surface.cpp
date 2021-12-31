#include "Surface.h"
#include "utils.h"
#include "Window.h"
Surface::Surface(std::shared_ptr<Instance> instance, std::shared_ptr<Window> window):mInstance(instance)
{
    printVkError(glfwCreateWindowSurface(mInstance->get(), window->getWindow(), nullptr, &mSurface), "create surface");
}

Surface::~Surface()
{
    vkDestroySurfaceKHR(mInstance->get(), mSurface, nullptr);
}

VkSurfaceKHR Surface::get() const
{
    return mSurface;
}
