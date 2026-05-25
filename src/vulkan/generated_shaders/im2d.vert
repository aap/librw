#version 450
layout(set = 0, binding = 0) uniform sampler2D tex0;
layout(push_constant) uniform Push { vec4 xform; vec4 matColor; vec4 alphaRef; } pc;
layout(location = 0) in vec4 in_pos;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_uv;
layout(location = 0) out vec4 v_color;
layout(location = 1) out vec2 v_uv;
void main() {
    gl_Position = in_pos;
    gl_Position.xy = gl_Position.xy * pc.xform.xy + pc.xform.zw;
    gl_Position.w = in_pos.w;
    gl_Position.xyz *= gl_Position.w;
    v_color = in_color * pc.matColor;
    v_uv = in_uv;
}
