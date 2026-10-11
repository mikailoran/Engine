$input a_position
$output v_ndc

#include <bgfx_shader.sh>

void main()
{
	// z = w puts the sky at depth 1 under either depth convention
	gl_Position = vec4(a_position.xy, 1.0, 1.0);
	v_ndc = a_position.xy;
}
