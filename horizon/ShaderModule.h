#include "utils.h"
#include <vector>
#include <fstream>
#include <string>

class Shader
{
public:
	Shader(VkDevice device,const char* path);
    ~Shader();
    VkShaderModule get()const;
private:
    static std::vector<char> readFile(const char* filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            spdlog::error("failed to open {}", filename);
        }
        size_t fileSize = (size_t)file.tellg();
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();

        return buffer;
    }

private:
    VkShaderModule mShaderModule;
    VkDevice mDevice;
};
