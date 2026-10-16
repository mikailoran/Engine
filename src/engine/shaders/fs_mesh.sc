$input v_wpos, v_normal, v_texcoord0

#include <bgfx_shader.sh>
#include "shadow.sh"
#include "tonemap.sh"

uniform vec4 u_time;
// Linear RGBA: the material's base color times the entity's tint.
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

// sRGB base color texture; 1x1 white for untextured submeshes.
SAMPLER2D(s_albedo, 0);

void main()
{
	vec3 n = normalize(v_normal);

	vec3 albedo = u_color.xyz * texture2D(s_albedo, v_texcoord0).xyz;
	vec3 v = normalize(u_eyePos.xyz - v_wpos);
	vec3 l = u_lightDir.xyz;
	vec3 h = normalize(l + v);

	float ndotl = dot(n, l);
	vec3 diffuse = albedo * u_lightColor.xyz * max(ndotl, 0.0);
	vec3 spec = u_lightColor.xyz * 0.25 * step(0.0, ndotl)
		* pow(max(dot(n, h), 0.0), 64.0);
	vec3 ambient = albedo * mix(u_groundColor.xyz, u_skyColor.xyz, n.y*0.5 + 0.5);

	float sun = shadowVisibility(v_wpos, n);
	vec3 lit = ambient + (diffuse + spec) * sun;

	gl_FragColor.xyz = toDisplay(lit);
	gl_FragColor.w = 1.0;
}
