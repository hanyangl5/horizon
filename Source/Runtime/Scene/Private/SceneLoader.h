struct LoadedScene
{
    MeshData meshData;
    Scene scene;
    std::vector<BoundingBox> worldBoxes;
    BoundingBox worldBounds;
};


LoadedScene loadDemoScene()
{
    LoadedScene loadedScene;
    loadBistro(loadedScene.meshData, loadedScene.scene);

    loadedScene.worldBoxes.resize(loadedScene.scene.globalTransform.size());
    for (auto &p : loadedScene.scene.meshForNode)
    {
        loadedScene.worldBoxes[p.first] =
            loadedScene.meshData.boxes[p.second].getTransformed(loadedScene.scene.globalTransform[p.first]);
    }

    loadedScene.worldBounds = loadedScene.worldBoxes.front();
    for (const auto &box : loadedScene.worldBoxes)
    {
        loadedScene.worldBounds.combinePoint(box.min_);
        loadedScene.worldBounds.combinePoint(box.max_);
    }
    return loadedScene;
}