#include "config.h"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

namespace TEST::SceneManagement {

using namespace Horizon;

class SceneManagementTest {
  public:
    SceneManagementTest() {
        EngineConfig config{};
        config.width = 800;
        config.height = 600;
        // config.asset_path =
        config.render_backend = RenderBackend::RENDER_BACKEND_VULKAN;
        config.offscreen = false;
        engine = std::make_unique<Engine>(config);
    }

  public:
    std::unique_ptr<Engine> engine{};
    std::string asset_path = "D:/codes/horizon/horizon/assets/";
};

TEST_CASE_FIXTURE(SceneManagementTest, "multithread mesh load") {

    auto &tp = engine->tp;
    constexpr u32 mesh_count = 500;
    std::vector<Mesh> meshes(mesh_count);

    std::vector<std::string> paths = {"D:/codes/horizon/horizon/assets/models/DamagedHelmet/"
                                      "DamagedHelmet.gltf",
                                      "D:/codes/horizon/horizon/assets/models/sponza/sponza.gltf",
                                      "D:/codes/horizon/horizon/assets/models/cerberus/cerberus.gltf"};

    std::vector<std::future<void>> results(mesh_count);

    LOG_INFO("start to load meshes");

    auto tp1 = std::chrono::high_resolution_clock::now();

    for (u32 i = 0; i < mesh_count; i++) {
        results[i] = std::move(tp->submit([&meshes, &paths, i]() {
            auto &m = meshes[i];
            m.LoadMesh(paths[i % 3]);
        }));
    }
    for (auto &res : results) {
        res.wait();
    }
    auto tp2 = std::chrono::high_resolution_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(tp2 - tp1).count();
    LOG_INFO("spend {} ms to load {} meshes", dur, mesh_count);
}


void t()  { throw std::runtime_error("ex"); }

TEST_CASE_FIXTURE(SceneManagementTest, "ex") {

    class A {
      public:
        virtual void vd(){};
        int a;
    };
    class B : public A {
      public:
        B() {
            c = 1;
            a = 2;
        }
        virtual void vd()  { c = 2;
        }
        int c;
    };
    A *a = new B;
    LOG_ERROR("{}", typeid(a).name());
    A *pa = new A;
    LOG_ERROR("{}", typeid(pa).name());
    auto b = dynamic_cast<A *>(a);
    LOG_ERROR("{}", typeid(b).name());
    {
        try {
             t();
         } catch (const std::exception &) {
             LOG_ERROR("e");
         }
     }
}
} // namespace TEST::SceneManagement