// Linear scene light to display colour. Include once per fragment shader.

// Scales scene light before the curve; the scene was lit for about 1.6.
#define TONEMAP_EXPOSURE 1.6

/**
 * @brief ACES filmic curve (Stephen Hill's fit) on linear sRGB @p color.
 *
 * Rolls highlights off toward white instead of clipping; 0..1 out.
 */
vec3 tonemapAces(vec3 color)
{
	// Linear sRGB to ACES' working space, with the RRT's saturation
	mat3 to_aces = mtxFromRows(
		vec3(0.59719, 0.35458, 0.04823),
		vec3(0.07600, 0.90834, 0.01566),
		vec3(0.02840, 0.13383, 0.83777) );
	mat3 from_aces = mtxFromRows(
		vec3( 1.60475, -0.53108, -0.07367),
		vec3(-0.10208,  1.10813, -0.00605),
		vec3(-0.00327, -0.07276,  1.07602) );

	vec3 v = mul(to_aces, color);
	vec3 a = v * (v + 0.0245786) - 0.000090537;
	vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
	return clamp(mul(from_aces, a / b), 0.0, 1.0);
}

/**
 * @brief Display colour for linear scene light @p color: exposure, the
 * filmic curve, then gamma 2.2. Replace this to change the tone mapper.
 */
vec3 toDisplay(vec3 color)
{
	vec3 mapped = tonemapAces(color * TONEMAP_EXPOSURE);
	return pow(mapped, vec3_splat(1.0/2.2) );
}
