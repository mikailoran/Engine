// The sun's shadow, from ShadowSystem. Include once per fragment shader.

// World to shadow map UV (xy) and depth (z).
uniform mat4 u_shadowMtx;
// x: normal offset in metres, y: depth bias, z: one texel in UV.
uniform vec4 u_shadowParams;

// The sun's depth map, compared LEQUAL when sampled.
SAMPLER2DSHADOW(s_shadowMap, 1);

/**
 * @brief Fraction of sunlight reaching world position @p wpos, 0 to 1.
 *
 * @p normal is the unit world normal. Fully lit outside the map. Replace
 * this to change the filter.
 */
float shadowVisibility(vec3 wpos, vec3 normal)
{
	// Offset along the normal so surfaces don't shadow themselves
	vec4 coord = mul(u_shadowMtx, vec4(wpos + normal * u_shadowParams.x, 1.0) );
	vec3 uvz = coord.xyz / coord.w;
	if (any(lessThan(uvz.xy, vec2_splat(0.0) ) )
	||  any(greaterThan(uvz.xy, vec2_splat(1.0) ) )
	||  uvz.z > 1.0)
	{
		return 1.0;
	}

	// 3x3 taps, each a hardware-filtered 2x2 compare
	float depth = uvz.z - u_shadowParams.y;
	float lit = 0.0;
	for (int y = -1; y <= 1; ++y)
	{
		for (int x = -1; x <= 1; ++x)
		{
			vec2 offset = vec2(float(x), float(y) ) * u_shadowParams.z;
			lit += shadow2D(s_shadowMap, vec3(uvz.xy + offset, depth) );
		}
	}
	return lit / 9.0;
}
