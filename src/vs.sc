$input a_position, a_color0
$output v_color0

#include <bgfx_shader.sh>

void main()
{
	// mul() rather than *: it hides the row/column-major difference between
	// the D3D and OpenGL backends. u_modelViewProj is supplied by bgfx.
	gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0) );
	v_color0 = a_color0;
}
