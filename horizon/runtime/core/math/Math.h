#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/ext.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Horizon {

	using u8 = uint8_t;
	using u16 = uint16_t;
	using u32 = uint32_t;
	using u64 = uint64_t;
	using i8 = int8_t;
	using i16 = int16_t;
	using i32 = int32_t;
	using i64 = int64_t;

	using f32 = float;
	using f64 = double;

	namespace Math = glm;
	inline glm::mat4 ReversePerspective(f32 _fov, f32 _aspect_ratio, f32 _near, f32 _far) {
		glm::mat4 ret = glm::perspective(_fov, _aspect_ratio, _near, _far);
		ret[2][2] = -_near / (_far - _near);
		ret[3][2] = (_far * _near) / (_far - _near);
		return ret;
	}
}