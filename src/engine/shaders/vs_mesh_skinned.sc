$input a_position, a_normal, a_texcoord0, a_indices, a_weight
$output v_wpos, v_normal, v_texcoord0

#include <bgfx_shader.sh>

// Skinning matrices (model x inverse bind), one per joint; kMaxJoints
uniform mat4 u_bones[128];

void main()
{
	// Blend the vertex's four joints into one bind-to-model matrix
	mat4 skin = a_weight.x * u_bones[int(a_indices.x)]
	          + a_weight.y * u_bones[int(a_indices.y)]
	          + a_weight.z * u_bones[int(a_indices.z)]
	          + a_weight.w * u_bones[int(a_indices.w)];
	vec4 posed = mul(skin, vec4(a_position, 1.0) );
	vec4 normal = mul(skin, vec4(a_normal, 0.0) );

	gl_Position = mul(u_modelViewProj, posed);
	v_wpos = mul(u_model[0], posed).xyz;
	v_normal = mul(u_model[0], vec4(normal.xyz, 0.0) ).xyz;
	v_texcoord0 = a_texcoord0;
}
