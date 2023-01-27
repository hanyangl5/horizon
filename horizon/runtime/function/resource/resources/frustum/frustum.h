/*****************************************************************/ /**
 * \file   FILE_NAME
 * \brief  BRIEF
 * 
 * \author XXX
 * \date   XXX
 *********************************************************************/

#pragma once

// standard libraries

// third party libraries

// project headers
#include <runtime/core/container/container.h>
#include <runtime/core/math/math.h>

namespace Horizon {

enum class Side { LEFT = 0, RIGHT = 1, TOP = 2, BOTTOM = 3, BACK = 4, FRONT = 5 };

class Frustum {
  public:
    Frustum() noexcept;
    ~Frustum() noexcept;

    Frustum(const Frustum &rhs) noexcept = default;
    Frustum &operator=(const Frustum &rhs) noexcept = default;
    Frustum(Frustum &&rhs) noexcept = default;
    Frustum &operator=(Frustum &&rhs) noexcept = default;

    /**
	 * @brief Updates the frustums planes based on a matrix
	 * @param matrix The matrix to update the frustum on
	 */
    void update(const math::Matrix44f &matrix);

    /**
	 * @brief Checks if a sphere is inside the Frustum
	 * @param pos The center of the sphere
	 * @param radius The radius of the sphere
	 */
    bool check_sphere(const math::Vector3f &pos, f32 radius);

    // bool CheckAABB(); TODO

    const Container::FixedArray<math::Vector4f, 6> &get_planes() const;

  private:
    std::array<math::Vector4f, 6> planes;
};

} // namespace Horizon
