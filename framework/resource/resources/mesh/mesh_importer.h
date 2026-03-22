#pragma once

namespace Horizon
{

class Mesh;

bool LoadMeshWithCgltf(Mesh &mesh);
bool LoadMeshWithFbxSdk(Mesh &mesh);

} // namespace Horizon
