uniform mat4 u_boneMatrices[64];

attribute vec3 in_pos;
attribute vec3 in_normal;
attribute vec4 in_color;
attribute vec2 in_tex0;
attribute vec4 in_weights;
attribute vec4 in_indices;

varying vec4 v_color;
varying vec2 v_tex0;
varying float v_fog;

void
main(void)
{
	vec3 SkinVertex = vec3(0.0, 0.0, 0.0);
	vec3 SkinNormal = vec3(0.0, 0.0, 0.0);
	for(int i = 0; i < 4; i++){
		SkinVertex += (u_boneMatrices[int(in_indices[i])] * vec4(in_pos, 1.0)).xyz * in_weights[i];
		SkinNormal += (mat3(u_boneMatrices[int(in_indices[i])]) * in_normal) * in_weights[i];
	}

	vec4 V = u_world * vec4(SkinVertex, 1.0);
	gl_Position = u_proj * u_view * V;
	vec3 N = mat3(u_world) * SkinNormal;

	v_color = in_color;
	v_color.rgb += u_ambLight.rgb*surfAmbient;

#ifdef DIRECTIONALS
	for(int i = 0; i < MAX_LIGHTS; i++){
		if(u_directLights[i].enabled == 0.0)
			break;
		v_color.rgb += DoDirLight(u_directLights[i], N)*surfDiffuse;
	}
#endif
#ifdef POINTLIGHTS
	for(int i = 0; i < MAX_LIGHTS; i++){
		if(u_pointLights[i].enabled == 0.0)
			break;
		v_color.rgb += DoPointLight(u_pointLights[i], V.xyz, N)*surfDiffuse;
	}
#endif
#ifdef SPOTLIGHTS
	for(int i = 0; i < MAX_LIGHTS; i++){
		if(u_spotLights[i].enabled == 0.0)
			break;
		v_color.rgb += DoSpotLight(u_spotLights[i], V.xyz, N)*surfDiffuse;
	}
#endif
	v_color *= u_matColor;

	v_tex0 = in_tex0;

	v_fog = DoFog(gl_Position.z);
}
