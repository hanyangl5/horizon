/*****************************************************************/ /**
 * \file   FILE_NAME
 * \brief  BRIEF
 * 
 * \author XXX
 * \date   XXX
 *********************************************************************/

// standard libraries

// third party libraries

// project headers

/* Copyright (c) 2019, Sascha Willems
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 the "License";
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "frustum.h"

// standard libraries

// third party libraries

// project headers

namespace Horizon {

void Frustum::update(const math::Matrix44f &matrix) {
    u32 left = static_cast<u32>(Side::LEFT);
    u32 right = static_cast<u32>(Side::RIGHT);
    u32 top = static_cast<u32>(Side::TOP);
    u32 bottom = static_cast<u32>(Side::BOTTOM);
    u32 front = static_cast<u32>(Side::FRONT);
    u32 back = static_cast<u32>(Side::BACK);

    planes[left].x() = matrix.at(0,3) + matrix.at(0,0);
    planes[left].y() = matrix.at(1,3) + matrix.at(1,0);
    planes[left].z() = matrix.at(2,3) + matrix.at(2,0);
    planes[left].w() = matrix.at(3,3) + matrix.at(3,0);

    planes[right].x() = matrix.at(0,3) - matrix.at(0,0);
    planes[right].y() = matrix.at(1,3) - matrix.at(1,0);
    planes[right].z() = matrix.at(2,3) - matrix.at(2,0);
    planes[right].w() = matrix.at(3,3) - matrix.at(3,0);

    planes[top].x() = matrix.at(0,3) - matrix.at(0,1);
    planes[top].y() = matrix.at(1,3) - matrix.at(1,1);
    planes[top].z() = matrix.at(2,3) - matrix.at(2,1);
    planes[top].w() = matrix.at(3,3) - matrix.at(3,1);

    planes[bottom].x() = matrix.at(0,3) + matrix.at(0,1);
    planes[bottom].y() = matrix.at(1,3) + matrix.at(1,1);
    planes[bottom].z() = matrix.at(2,3) + matrix.at(2,1);
    planes[bottom].w() = matrix.at(3,3) + matrix.at(3,1);

    planes[back].x() = matrix.at(0,3) + matrix.at(0,2);
    planes[back].y() = matrix.at(1,3) + matrix.at(1,2);
    planes[back].z() = matrix.at(2,3) + matrix.at(2,2);
    planes[back].w() = matrix.at(3,3) + matrix.at(3,2);

    planes[front].x() = matrix.at(0,3) - matrix.at(0,2);
    planes[front].y() = matrix.at(1,3) - matrix.at(1,2);
    planes[front].z() = matrix.at(2,3) - matrix.at(2,2);
    planes[front].w() = matrix.at(3,3) - matrix.at(3,2);

    for (size_t i = 0; i < planes.size(); i++) {
        float length = sqrtf(planes[i].x() * planes[i].x() + planes[i].y() * planes[i].y() + planes[i].z() * planes[i].z());
        planes[i] /= length;
    }
}

bool Frustum::check_sphere(const math::Vector3f& pos, f32 radius) {
    for (size_t i = 0; i < planes.size(); i++) {
        if ((planes[i].x() * pos.x()) + (planes[i].y() * pos.y()) + (planes[i].z() * pos.z()) + planes[i].w() <= -radius) {
            return false;
        }
    }
    return true;
}
const Container::FixedArray<math::Vector4f, 6> &Frustum::get_planes() const { return planes; }

} // namespace Horizon
