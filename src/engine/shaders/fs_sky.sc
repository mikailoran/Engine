$input v_ndc

#include <bgfx_shader.sh>
#include "tonemap.sh"

// Linear RGB sky straight up, at the horizon, and below it.
uniform vec4 u_skyZenith;
uniform vec4 u_skyHorizon;
uniform vec4 u_skyGround;
// x: cos of the sun's angular radius, y: glow sharpness, z: gradient
// falloff, w: glow strength.
uniform vec4 u_skyParams;
// World direction toward the sun, normalized.
uniform vec4 u_lightDir;
// Sun color times intensity.
uniform vec4 u_lightColor;

/**
 * @brief Linear RGB seen along the unit world direction @p dir.
 *
 * A gradient from horizon to zenith, the ground below, and the sun's disc
 * and glow. Replace this to change the sky model.
 */
vec3 skyRadiance(vec3 dir)
{
	vec3 sky = mix(u_skyHorizon.xyz, u_skyZenith.xyz,
		pow(max(dir.y, 0.0), u_skyParams.z));

	float cos_angle = dot(dir, u_lightDir.xyz);
	float cos_radius = u_skyParams.x;
	// Soften the disc's edge over a tenth of its size
	float disc = smoothstep(cos_radius - (1.0 - cos_radius) * 0.1, cos_radius,
		cos_angle);
	float glow = u_skyParams.w * exp((cos_angle - 1.0) * u_skyParams.y);
	sky += u_lightColor.xyz * (disc + glow);

	// A narrow band blends the horizon into the ground, hiding the sun
	float below = smoothstep(0.0, 0.02, -dir.y);
	return mix(sky, u_skyGround.xyz, below);
}

void main()
{
	// Unproject this pixel at two depths valid in both depth conventions
	vec4 ray_start = mul(u_invViewProj, vec4(v_ndc, 0.0, 1.0));
	vec4 ray_end = mul(u_invViewProj, vec4(v_ndc, 1.0, 1.0));
	vec3 dir = normalize(ray_end.xyz / ray_end.w - ray_start.xyz / ray_start.w);

	vec3 color = skyRadiance(dir);
	gl_FragColor = vec4(toDisplay(color), 1.0);
}
