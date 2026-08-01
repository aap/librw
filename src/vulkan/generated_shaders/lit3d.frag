#version 450

// Ported from the GL3 shaders (header.frag + simple.frag).

#define MAX_LIGHTS 8

layout(set = 0, binding = 0) uniform sampler2D tex0;

layout(set = 1, binding = 0) uniform Lit3D {
    mat4 mvp;
    mat4 world;
    vec4 ambient;
    vec4 lightParams[MAX_LIGHTS];
    vec4 lightColor[MAX_LIGHTS];
    vec4 lightPosition[MAX_LIGHTS];
    vec4 lightDirection[MAX_LIGHTS];
    mat4 texMatrix;
    vec4 colorClamp;
    vec4 envColor;
    vec4 fxParams;
    vec4 fogData;
    vec4 fogColor;
} u;

layout(push_constant) uniform Push {
    vec4 matColor;
    vec4 surfProps;	// x=ambient, y=diffuse, z=lighting on
    vec4 alphaRef;
} pc;

layout(location = 0) in vec4 v_color;
layout(location = 1) in vec2 v_tex0;
layout(location = 2) in float v_fog;

layout(location = 0) out vec4 out_color;

void main()
{
    vec4 color = v_color * texture(tex0, v_tex0);
    color.rgb = mix(u.fogColor.rgb, color.rgb, v_fog);
    if(color.a < pc.alphaRef.x || color.a >= pc.alphaRef.y)
        discard;
    out_color = color;
}
