#include <vulkan/vulkan.hpp>

class Instance
{
public:
    Instance(const uint32_t enabledExtensionCount, const std::vector<std::string>& enabledExtension);
    ~Instance();
    VkInstance get()const;
private:
    VkInstance mInstance;
};
