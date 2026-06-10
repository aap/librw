#version 460
#extension GL_EXT_ray_query : require

layout(set = 0, binding = 0) uniform sampler2D tex0;
layout(set = 1, binding = 0) uniform accelerationStructureEXT rayScene;
layout(push_constant) uniform Push { mat4 mvp; vec4 matColor; vec4 alphaRef; vec4 rtParams; vec4 rtLight; } pc;
layout(location = 0) in vec4 v_color;
layout(location = 1) in vec2 v_uv;
layout(location = 2) in vec3 v_world_pos;
layout(location = 3) in vec3 v_world_normal;
layout(location = 0) out vec4 out_color;

void main() {
    vec4 tex = texture(tex0, v_uv);
    vec4 color = v_color * tex;
    if (color.a < pc.alphaRef.x || color.a >= pc.alphaRef.y) discard;

    if (pc.rtParams.x > 0.5) {
        vec3 normal = normalize(v_world_normal);
        vec3 lightDir = normalize(pc.rtLight.xyz);
        float facing = max(dot(normal, lightDir), 0.0);
        if (facing > 0.02) {
            rayQueryEXT rq;
            vec3 origin = v_world_pos + normal * 0.18 + lightDir * 0.08;
            uint flags = gl_RayFlagsOpaqueEXT |
                         gl_RayFlagsTerminateOnFirstHitEXT |
                         gl_RayFlagsSkipClosestHitShaderEXT;
            rayQueryInitializeEXT(rq, rayScene, flags, 0xFF, origin, 0.12, lightDir, pc.rtLight.w);
            while (rayQueryProceedEXT(rq)) {
            }
            if (rayQueryGetIntersectionTypeEXT(rq, true) != gl_RayQueryCommittedIntersectionNoneEXT) {
                color.rgb *= mix(1.0, 0.52, pc.rtParams.y);
            }
        }
    }

    out_color = color;
}
