#version 450

// Ported from the GL3 matfx_env.vert.

#define MAX_LIGHTS 8

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
    vec4 fogData;	// x=start, y=end, z=range, w=disable
    vec4 fogColor;
} u;

layout(push_constant) uniform Push {
    vec4 matColor;
    vec4 surfProps;	// x=ambient, y=diffuse, z=lighting on
    vec4 alphaRef;
} pc;

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec4 in_color;
layout(location = 3) in vec2 in_uv;

layout(location = 0) out vec4 v_color;
layout(location = 1) out vec2 v_tex0;
layout(location = 2) out vec2 v_tex1;
layout(location = 3) out vec4 v_envColor;
layout(location = 4) out float v_fog;

#define surfAmbient (pc.surfProps.x)
#define surfDiffuse (pc.surfProps.y)
#define lightingOn (pc.surfProps.z)

#define fogEnd (u.fogData.y)
#define fogRange (u.fogData.z)
#define fogDisable (u.fogData.w)

vec3 DoDynamicLight(vec3 V, vec3 N)
{
    vec3 color = vec3(0.0);
    for(int i = 0; i < MAX_LIGHTS; i++){
        if(u.lightParams[i].x == 0.0)
            break;
        if(u.lightParams[i].x == 1.0){
            float l = max(0.0, dot(N, -u.lightDirection[i].xyz));
            color += l*u.lightColor[i].rgb;
        }else if(u.lightParams[i].x == 2.0){
            vec3 dir = V - u.lightPosition[i].xyz;
            float dist = length(dir);
            float atten = max(0.0, 1.0 - dist/max(u.lightParams[i].y, 0.0001));
            float l = max(0.0, dot(N, -normalize(dir)));
            color += l*u.lightColor[i].rgb*atten;
        }else if(u.lightParams[i].x == 3.0){
            vec3 dir = V - u.lightPosition[i].xyz;
            float dist = length(dir);
            float atten = max(0.0, 1.0 - dist/max(u.lightParams[i].y, 0.0001));
            dir /= max(dist, 0.0001);
            float l = max(0.0, dot(N, -dir));
            float pcos = dot(dir, u.lightDirection[i].xyz);
            float ccos = -u.lightParams[i].z;
            float falloff = (pcos - ccos)/max(1.0 - ccos, 0.0001);
            if(falloff < 0.0)
                l = 0.0;
            l *= max(falloff, u.lightParams[i].w);
            color += l*u.lightColor[i].rgb*atten;
        }
    }
    return color;
}

float DoFog(float w)
{
    return clamp((w - fogEnd)*fogRange, fogDisable, 1.0);
}

void main()
{
    vec4 Vertex = u.world * vec4(in_pos, 1.0);
    gl_Position = u.mvp * vec4(in_pos, 1.0);
    gl_Position.y = -gl_Position.y;
    vec3 Normal = mat3(u.world) * in_normal;

    v_tex0 = in_uv;
    v_tex1 = (u.texMatrix * vec4(Normal, 1.0)).xy;

    vec4 color = in_color;
    if(lightingOn > 0.5){
        color.rgb += u.ambient.rgb*surfAmbient;
        color.rgb += DoDynamicLight(Vertex.xyz, Normal)*surfDiffuse;
    }
    color = clamp(color, 0.0, 1.0);
    v_envColor = max(color, u.colorClamp) * u.envColor;
    v_color = color * pc.matColor;

    v_fog = DoFog(gl_Position.w);
}
