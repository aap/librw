#version 330

uniform sampler2D tex;

in vec4 v_color;
in vec2 v_tex0;

out vec4 color;

void
main(void)
{
	color = v_color*texture2D(tex, vec2(v_tex0.x, v_tex0.y));
}

