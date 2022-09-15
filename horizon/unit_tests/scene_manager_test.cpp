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
    std::string asset_path = "C:/FILES/horizon/horizon/assets/";
};


TEST_CASE_FIXTURE(SceneManagementTest, "multithread mesh load benchmark") {

    auto &tp = engine->tp;
    constexpr u32 mesh_count = 500;
    std::vector<Mesh> meshes(mesh_count);

    std::vector<std::string> paths = {asset_path + "models/DamagedHelmet/DamagedHelmet.gltf",
                                      asset_path + "models/sponza/sponza.gltf",
                                      asset_path + "models/cerberus/cerberus.gltf"};

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
    LOG_INFO("spend {} ms to load {} meshes using bs thread pool", dur, mesh_count);

    // use intel tbb
    auto LoadMesh = [&meshes, &paths](const tbb::blocked_range<u32> &r) {
        for (int v = r.begin(); v < r.end(); v++) {
            auto &m = meshes[v];
            m.LoadMesh(paths[v % 3]);
        }
    };

    LOG_INFO("start to load meshes");

    tp1 = std::chrono::high_resolution_clock::now();

    tbb::parallel_for(tbb::blocked_range<u32>(0, mesh_count), LoadMesh);

    tp2 = std::chrono::high_resolution_clock::now();
    dur = std::chrono::duration_cast<std::chrono::milliseconds>(tp2 - tp1).count();
    LOG_INFO("spend {} ms to load {} meshes using tbb", dur, mesh_count);
}




} // namespace TEST::SceneManagement