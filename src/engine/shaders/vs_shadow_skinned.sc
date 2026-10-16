$input a_position, a_indices, a_weight

#include <bgfx_shader.sh>

// Skinning matrices (model x inverse bind), one per joint; kMaxJoints
uniform mat4 u_bones[128];

void main()
{
	mat4 skin = a_weight.x * u_bones[int(a_indices.x)]
	          + a_weight.y * u_bones[int(a_indices.y)]
	          + a_weight.z * u_bones[int(a_indices.z)]
	          + a_weight.w * u_bones[int(a_indices.w)];
	gl_Position = mul(u_modelViewProj, mul(skin, vec4(a_position, 1.0) ) );
}
