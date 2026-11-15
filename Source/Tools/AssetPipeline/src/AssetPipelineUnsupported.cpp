/*
 * Copyright (c) 2026 Horizon
 */

#include "AssetPipeline.h"

#include "Core/ILog.h"

bool ProcessTextures(AssetPipelineParams*, ProcessTexturesParams*)
{
    LOGF(Error, "Texture cooking is unavailable because compression backends are not vendored in this repository.");
    return true;
}

bool ProcessAnimations(AssetPipelineParams*, ProcessAnimationsParams*)
{
    LOGF(Error, "Animation cooking is unavailable because the legacy cooker has not been migrated to the current Ozz API.");
    return true;
}

void ReleaseSkeletonAndAnimationParams(SkeletonAndAnimations*) {}
