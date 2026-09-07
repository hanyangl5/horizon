#pragma once

#include "Core/IFileSystem.h"

struct SceneAssetError;

// Requires initialized memory, logging and filesystem services. Uses existing
// resource directory mappings; does not change them. Call serially on the main thread.
// Returns true for both an unchanged cache and a successful cook.
bool ensureSceneGltfCooked(ResourceDirectory sourceDirectory, const char* pSourceFile,
                           ResourceDirectory outputDirectory, SceneAssetError* pError);
