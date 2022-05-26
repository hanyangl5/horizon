#include "Texture.h"

namespace Horizon {
	namespace RHI {
		Texture2::Texture2(const TextureCreateInfo& texture_create_info) :
			m_type(texture_create_info.texture_type),
			m_format(texture_create_info.texture_format),
			m_usage(texture_create_info.texture_usage),
			m_width(texture_create_info.width),
			m_height(texture_create_info.height),
			m_depth(texture_create_info.depth)
		{

		}

	}
}