$input v_wpos, v_normal, v_texcoord0

#include <bgfx_shader.sh>
#include "shadow.sh"
#include "tonemap.sh"

#define PI 3.14159265

uniform vec4 u_time;
// Linear RGBA: the material's base color times the entity's tint.
uniform vec4 u_color;
// x: metallic, y: perceptual roughness, both 0 to 1.
uniform vec4 u_material;
// Linear RGB light given off, may exceed 1; w unused.
uniform vec4 u_emissive;
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
// sRGB emissive texture; 1x1 white when the material has none.
SAMPLER2D(s_emissive, 2);

/** @brief Ambient light arriving from around unit world direction @p dir. */
vec3 hemisphere(vec3 dir)
{
	return mix(u_groundColor.xyz, u_skyColor.xyz, dir.y*0.5 + 0.5);
}

/**
 * @brief GGX specular from one light, times pi: D * V * F.
 *
 * @p alpha is roughness squared; the dot products are clamped above 0.
 */
vec3 specularGgx(vec3 f0, float alpha, float ndoth, float ndotv, float ndotl,
	float vdoth)
{
	float a2 = alpha * alpha;
	float d = ndoth * ndoth * (a2 - 1.0) + 1.0;
	// Times pi, matching the diffuse term's convention below
	float distribution = a2 / (d * d);
	// Height-correlated Smith visibility
	float visibility = 0.5 / (ndotl * sqrt(ndotv * ndotv * (1.0 - a2) + a2)
		+ ndotv * sqrt(ndotl * ndotl * (1.0 - a2) + a2) );
	vec3 fresnel = f0 + (1.0 - f0) * pow(1.0 - vdoth, 5.0);
	return distribution * visibility * fresnel;
}

/**
 * @brief Fraction of ambient light a surface reflects specularly, by
 * reflectance @p f0 (Karis' fit of the split-sum lookup).
 */
vec3 ambientSpecular(vec3 f0, float roughness, float ndotv)
{
	const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
	const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
	vec4 r = roughness * c0 + c1;
	float a004 = min(r.x * r.x, exp2(-9.28 * ndotv) ) * r.x + r.y;
	vec2 ab = vec2(-1.04, 1.04) * a004 + r.zw;
	return f0 * ab.x + ab.y;
}

void main()
{
	vec3 n = normalize(v_normal);

	vec3 base = u_color.xyz * texture2D(s_albedo, v_texcoord0).xyz;
	float metallic = u_material.x;
	// Too smooth makes the sun a sub-pixel highlight that flickers
	float roughness = max(u_material.y, 0.045);
	float alpha = roughness * roughness;

	// Dielectrics reflect 4% head-on; metals tint by their base color
	vec3 f0 = mix(vec3_splat(0.04), base, metallic);
	vec3 diffuse_color = base * (1.0 - metallic);

	vec3 v = normalize(u_eyePos.xyz - v_wpos);
	vec3 l = u_lightDir.xyz;
	vec3 h = normalize(l + v);
	float ndotl = max(dot(n, l), 0.0);
	float ndotv = max(dot(n, v), 1e-4);
	float ndoth = max(dot(n, h), 0.0);
	float vdoth = max(dot(v, h), 0.0);

	// The sun's color is the lit diffuse of white, so Lambert drops its 1/pi
	vec3 spec = specularGgx(f0, alpha, ndoth, ndotv, ndotl, vdoth);
	vec3 direct = (diffuse_color + spec) * u_lightColor.xyz * ndotl;

	// Rough surfaces reflect the ambient around the normal, smooth along r
	vec3 r = reflect(-v, n);
	vec3 env_dir = normalize(mix(r, n, alpha) );
	vec3 ambient = diffuse_color * hemisphere(n)
		+ ambientSpecular(f0, roughness, ndotv) * hemisphere(env_dir);

	float sun = shadowVisibility(v_wpos, n);
	vec3 emission = u_emissive.xyz * texture2D(s_emissive, v_texcoord0).xyz;
	vec3 lit = ambient + direct * sun + emission;

	gl_FragColor.xyz = toDisplay(lit);
	gl_FragColor.w = 1.0;
}
