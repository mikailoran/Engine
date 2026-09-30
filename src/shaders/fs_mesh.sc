$input v_wpos, v_normal

#include <bgfx_shader.sh>

uniform vec4 u_time;
// Linear RGBA surface color, set per draw by RenderSystem.
uniform vec4 u_color;
// Camera world position.
uniform vec4 u_eyePos;
// World direction toward the sun, normalized.
uniform vec4 u_lightDir;
// Sun color times intensity.
uniform vec4 u_lightColor;
// Hemisphere ambient for up- and down-facing normals.
uniform vec4 u_skyColor;
uniform vec4 u_groundColor;

void main()
{
	vec3 albedo = u_color.xyz;
	vec3 n = normalize(v_normal);
	vec3 v = normalize(u_eyePos.xyz - v_wpos);
	vec3 l = u_lightDir.xyz;
	vec3 h = normalize(l + v);

	float ndotl = dot(n, l);
	vec3 diffuse = albedo * u_lightColor.xyz * max(ndotl, 0.0);
	vec3 spec = u_lightColor.xyz * 0.25 * step(0.0, ndotl)
		* pow(max(dot(n, h), 0.0), 64.0);
	vec3 ambient = albedo * mix(u_groundColor.xyz, u_skyColor.xyz, n.y*0.5 + 0.5);

	gl_FragColor.xyz = pow(ambient + diffuse + spec, vec3_splat(1.0/2.2) );
	gl_FragColor.w = 1.0;
}
