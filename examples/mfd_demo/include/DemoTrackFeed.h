/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

#include "mfd/runtime/SceneRegistry.h"

namespace mfd
{
class DemoTrackFeed
{
public:
    void Update(float elapsedSeconds, SceneRegistry& scene);

private:
    int previousTrackCount_ = 0;
};
} // namespace mfd
