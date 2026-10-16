$input a_position
$output v_texcoord0

#include <bgfx_shader.sh>

// x: dot radius, y: half the quad's size, in px.
uniform vec4 u_crossExtra;

void main()
{
	// Centre on a pixel corner, so even-width arms stay crisp
	vec2 size = u_viewRect.zw;
	vec2 offset = a_position.xy * u_crossExtra.y;
	vec2 pixel = floor(size * 0.5) + offset;
	gl_Position = vec4(pixel.x / size.x * 2.0 - 1.0,
		1.0 - pixel.y / size.y * 2.0, 0.0, 1.0);
	// Offset from the centre in px, for the fragment shader's shape
	v_texcoord0 = offset;
}
