float4x4	combinedMat	: register(c0);
float4x4	worldMat	: register(c4);
float3x3	normalMat	: register(c8);
float4		matCol		: register(c12);
float4		surfProps	: register(c13);
float4		ambientLight	: register(c14);

#define surfAmbient (surfProps.x)
#define surfSpecular (surfProps.y)
#define surfDiffuse (surfProps.z)
#define surfPrelight (surfProps.w)

#include "lighting.h"

int numDirLights : register(i0);
int numPointLights : register(i1);
int numSpotLights : register(i2);
int4 firstLight : register(c15);
Light lights[8] : register(c16);

float4x4	texMat	: register(c40);

#define firstDirLight (firstLight.x)
#define firstPointLight (firstLight.y)
#define firstSpotLight (firstLight.z)

struct VS_in
{
	float4 Position		: POSITION;
	float3 Normal		: NORMAL;
	float2 TexCoord		: TEXCOORD0;
	float4 Prelight		: COLOR0;
};

struct VS_out {
	float4 Position		: POSITION;
	float2 TexCoord0	: TEXCOORD0;
	float2 TexCoord1	: TEXCOORD1;
	float4 Color		: COLOR0;
};


VS_out main(in VS_in input)
{
	VS_out output;

	output.Position = mul(combinedMat, input.Position);
	float3 Vertex = mul(worldMat, input.Position).xyz;
	float3 Normal = mul(normalMat, input.Normal);

	output.TexCoord0 = input.TexCoord;
	output.TexCoord1 = mul(texMat, float4(Normal, 1.0)).xy;

	output.Color = float4(0.0, 0.0, 0.0, 1.0);
	if(surfPrelight > 0.0)
		output.Color = input.Prelight;

	output.Color.rgb += ambientLight.rgb * surfAmbient;

	int i;
#ifdef DIRECTIONALS
	for(i = 0; i < numDirLights; i++)
		output.Color.xyz += DoDirLight(lights[i+firstDirLight], Normal)*surfDiffuse;
#endif
#ifdef POINTLIGHTS
	for(i = 0; i < numPointLights; i++)
		output.Color.xyz += DoPointLight(lights[i+firstPointLight], Vertex.xyz, Normal)*surfDiffuse;
#endif
#ifdef SPOTLIGHTS
	for(i = 0; i < numSpotLights; i++)
		output.Color.xyz += DoSpotLight(lights[i+firstSpotLight], Vertex.xyz, Normal)*surfDiffuse;
#endif
	// PS2 clamps before material color
	output.Color = clamp(output.Color, 0.0, 1.0);
	output.Color *= matCol;

	return output;
}
