$input a_position, a_normal
$output v_wpos, v_normal

#include <bgfx_shader.sh>

void main()
{
	gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0) );
	v_wpos = mul(u_model[0], vec4(a_position, 1.0) ).xyz;
	// Unpack Uint8 normals (geometryc PACKNORMAL 1)
	vec3 normal = a_normal.xyz*2.0 - 1.0;
	v_normal = mul(u_model[0], vec4(normal, 0.0) ).xyz;
}
