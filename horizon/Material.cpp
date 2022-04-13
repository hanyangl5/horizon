#include "Material.h"

namespace Horizon {

	void Material::UpdateDescriptorSet()
	{
		m_material_ub->update(&m_material_ubdata, sizeof(m_material_ubdata));

		DescriptorSetUpdateDesc desc;
		desc.bindResource(0, m_material_ub);
		desc.bindResource(1, base_color_texture);
		desc.bindResource(2, normal_texture);
		desc.bindResource(3, metallic_rougness_texture);

		m_material_descriptor_set->allocateDescriptorSet();
		m_material_descriptor_set->UpdateDescriptorSet(desc);
	}

}