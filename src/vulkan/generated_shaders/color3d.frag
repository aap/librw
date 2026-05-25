#version 450
layout(set = 0, binding = 0) uniform sampler2D tex0;
layout(push_constant) uniform Push { mat4 mvp; vec4 matColor; vec4 alphaRef; } pc;
layout(location = 0) in vec4 v_color;
layout(location = 1) in vec2 v_uv;
layout(location = 0) out vec4 out_color;
void main() {
    vec4 tex = texture(tex0, v_uv);
    vec4 color = v_color * tex;
    if (color.a < pc.alphaRef.x || color.a >= pc.alphaRef.y) discard;
    out_color = color;
}
