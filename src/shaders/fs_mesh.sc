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
// x: texture repeats per world unit.
uniform vec4 u_texParams;

// sRGB albedo texture, tinted by u_color.
SAMPLER2D(s_albedo, 0);

void main()
{
	vec3 n = normalize(v_normal);

	// World-space triplanar: project along each axis, blend by the normal
	vec3 w = pow(abs(n), vec3_splat(4.0) );
	w /= w.x + w.y + w.z;
	vec3 wpos = v_wpos * u_texParams.x;
	vec3 tex = texture2D(s_albedo, wpos.zy).xyz * w.x
		+ texture2D(s_albedo, wpos.xz).xyz * w.y
		+ texture2D(s_albedo, wpos.xy).xyz * w.z;

	vec3 albedo = u_color.xyz * tex;
	vec3 v = normalize(u_eyePos.xyz - v_wpos);
	vec3 l = u_lightDir.xyz;
	vec3 h = normalize(l + v);

	float ndotl = dot(n, l);
	vec3 diffuse = albedo * u_lightColor.xyz * max(ndotl, 0.0);
	vec3 spec = u_lightColor.xyz * 0.25 * step(0.0, ndotl)
		* pow(max(dot(n, h), 0.0), 64.0);
	vec3 ambient = albedo * mix(u_groundColor.xyz, u_skyColor.xyz, n.y*0.5 + 0.5);

	vec3 lit = ambient + diffuse + spec;

	gl_FragColor.xyz = pow(lit, vec3_splat(1.0/2.2) );
	gl_FragColor.w = 1.0;
}
