#include <bgfx_shader.sh>

// Depth only: the shadow framebuffer has no color target
void main()
{
	gl_FragColor = vec4_splat(0.0);
}
