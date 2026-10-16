$input v_texcoord0

#include <bgfx_shader.sh>

// x: gap, y: arm length, z: thickness, w: outline, all in px.
uniform vec4 u_crossShape;
// x: dot radius, y: half the quad's size, in px.
uniform vec4 u_crossExtra;
// Display RGBA of the fill and the border.
uniform vec4 u_crossColor;
uniform vec4 u_crossOutline;

/** @brief Signed distance from @p p to a box at @p centre. */
float boxDistance(vec2 p, vec2 centre, vec2 half_size)
{
	vec2 q = abs(p - centre) - half_size;
	return length(max(q, vec2_splat(0.0))) + min(max(q.x, q.y), 0.0);
}

/**
 * @brief Signed distance in px from @p p to the crosshair's fill.
 *
 * @p p is folded into one quadrant, so the shape must be symmetric in x and y.
 * Replace this to change the crosshair's shape.
 */
float crosshairDistance(vec2 p)
{
	float gap = u_crossShape.x;
	float len = u_crossShape.y;
	float half_thick = u_crossShape.z * 0.5;

	float d = 1.0e6;
	if (len > 0.0)
	{
		float arm_x = boxDistance(p, vec2(gap + len * 0.5, 0.0),
			vec2(len * 0.5, half_thick));
		float arm_y = boxDistance(p, vec2(0.0, gap + len * 0.5),
			vec2(half_thick, len * 0.5));
		d = min(arm_x, arm_y);
	}
	if (u_crossExtra.x > 0.0)
	{
		d = min(d, length(p) - u_crossExtra.x);
	}
	return d;
}

void main()
{
	float d = crosshairDistance(abs(v_texcoord0));
	// One pixel of antialiasing at each edge
	float fill = clamp(0.5 - d, 0.0, 1.0);
	float shape = clamp(0.5 - (d - u_crossShape.w), 0.0, 1.0);

	vec4 color = mix(u_crossOutline, u_crossColor, fill);
	gl_FragColor = vec4(color.rgb, color.a * shape);
}
