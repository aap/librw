#version 450

// Ported from the GL3 matfx_env.frag.

#define MAX_LIGHTS 8

layout(set = 0, binding = 0) uniform sampler2D tex0;
layout(set = 2, binding = 0) uniform sampler2D tex1;

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
layout(location = 2) in vec2 v_tex1;
layout(location = 3) in vec4 v_envColor;
layout(location = 4) in float v_fog;

layout(location = 0) out vec4 out_color;

#define shininess (u.fxParams.x)
#define disableFBA (u.fxParams.y)
#define forceOpaque (u.fxParams.z)

void main()
{
    vec4 pass1 = v_color * texture(tex0, v_tex0);
    pass1.a = max(pass1.a, forceOpaque);
    vec4 pass2 = v_envColor * shininess * texture(tex1, v_tex1);

    pass1.rgb = mix(u.fogColor.rgb, pass1.rgb, v_fog);
    pass2.rgb = mix(vec3(0.0), pass2.rgb, v_fog);

    float fba = max(pass1.a, disableFBA);
    vec4 color;
    color.rgb = pass1.rgb*pass1.a + pass2.rgb*fba;
    color.a = pass1.a;

    if(color.a < pc.alphaRef.x || color.a >= pc.alphaRef.y)
        discard;

    out_color = color;
}
