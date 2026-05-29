#version 450
layout(set = 0, binding = 0) uniform sampler2D tex0;
layout(push_constant) uniform Push { mat4 mvp; vec4 matColor; vec4 alphaRef; vec4 rtParams; vec4 rtLight; } pc;
layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec4 in_color;
layout(location = 3) in vec2 in_uv;
layout(location = 0) out vec4 v_color;
layout(location = 1) out vec2 v_uv;
layout(location = 2) out vec3 v_world_pos;
layout(location = 3) out vec3 v_world_normal;
void main() {
    gl_Position = pc.mvp * vec4(in_pos, 1.0);
    gl_Position.y = -gl_Position.y;
    v_color = in_color * pc.matColor;
    v_uv = in_uv;
    v_world_pos = in_pos;
    v_world_normal = in_normal;
}
