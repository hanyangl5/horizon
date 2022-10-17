
namespace Horizon::MaterialEditor {

enum class MaterialModel { COOK_TORRANCE, DISNEY12, HORIZON };

struct HorizonMaterial {
    std::string name;
    MaterialModel material_model;
};


// https://zhuanlan.zhihu.com/p/20693043
struct HorizonMaterialHeader {
    unsigned int magic_number;
    unsigned int version;
    unsigned int material_model;
};

struct HorizonMaterialEnd {
    unsigned int magic_number;
};

}