#include "Material.h"

namespace Horizon {

	void Material::UpdateDescriptorSet() noexcept
	{
		m_material_ub->update(&m_material_ubdata, sizeof(m_material_ubdata));

		DescriptorSetUpdateDesc desc;
		desc.BindResource(0, m_material_ub);
		desc.BindResource(1, base_color_texture);
		desc.BindResource(2, normal_texture);
		desc.BindResource(3, metallic_rougness_texture);

		m_material_descriptor_set->UpdateDescriptorSet(desc);
	}

}