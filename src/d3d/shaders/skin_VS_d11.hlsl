cbuffer StandardConstants : register(b0)
{
	float4x4 combinedMat : packoffset(c0);
	float4x4 worldMat : packoffset(c4);
	float3x3 normalMat : packoffset(c8);
	float4 matCol : packoffset(c12);
	float4 surfProps : packoffset(c13);
	float4 fogData : packoffset(c14);
	float4 ambientLight : packoffset(c15);
};

struct Light
{
	float4 color;
	float4 position;
	float4 direction;
};

cbuffer LightConstants : register(b1)
{
	int4 numLights;
	int4 firstLight;
	Light lights[8];
};

cbuffer SkinConstants : register(b2)
{
	float4x3 boneMatrices[64];
};

#define surfAmbient (surfProps.x)
#define surfDiffuse (surfProps.z)

#define numDirLights (numLights.x)
#define numPointLights (numLights.y)
#define numSpotLights (numLights.z)

#define firstDirLight (firstLight.x)
#define firstPointLight (firstLight.y)
#define firstSpotLight (firstLight.z)

float3 DoDirLight(Light light, float3 normal)
{
	float strength = max(0.0f, dot(normal, -light.direction.xyz));
	return strength * light.color.xyz;
}

float3 DoPointLight(Light light, float3 vertex, float3 normal)
{
	float3 direction = vertex - light.position.xyz;
	float distance = length(direction);
	float attenuation = max(0.0f, 1.0f - distance / light.color.w);
	float strength = max(0.0f, dot(normal, -normalize(direction)));
	return strength * light.color.xyz * attenuation;
}

float3 DoSpotLight(Light light, float3 vertex, float3 normal)
{
	float3 direction = vertex - light.position.xyz;
	float distance = length(direction);
	float attenuation = max(0.0f, 1.0f - distance / light.color.w);
	direction /= distance;
	float strength = max(0.0f, dot(normal, -direction));
	float pointCos = dot(direction, light.direction.xyz);
	float coneCos = -light.position.w;
	float falloff = (pointCos - coneCos) / (1.0f - coneCos);
	if(falloff < 0.0f)
		strength = 0.0f;
	strength *= max(falloff, light.direction.w);
	return strength * light.color.xyz * attenuation;
}

struct VSInput
{
	float4 Position : POSITION;
	float3 normal : NORMAL;
	float4 color : COLOR0;
	float2 texcoord : TEXCOORD0;
	float4 weights : BLENDWEIGHT0;
	uint4 indices : BLENDINDICES0;
};

struct VSOutput
{
	float4 Position : SV_POSITION;
	float4 color : COLOR0;
	float2 texcoord : TEXCOORD0;
};

VSOutput main(VSInput input)
{
	VSOutput output;

	float3 skinVertex = float3(0.0f, 0.0f, 0.0f);
	float3 skinNormal = float3(0.0f, 0.0f, 0.0f);
	for(int j = 0; j < 4; j++){
		skinVertex += mul(input.Position,
			boneMatrices[input.indices[j]]).xyz * input.weights[j];
		skinNormal += mul(input.normal,
			(float3x3)boneMatrices[input.indices[j]]).xyz * input.weights[j];
	}

	output.Position = mul(combinedMat, float4(skinVertex, 1.0f));
	float3 vertex = mul(worldMat, float4(skinVertex, 1.0f)).xyz;
	float3 normal = mul(normalMat, skinNormal);

	output.color = input.color;
	output.color.rgb += ambientLight.rgb * surfAmbient;

	int i;
#ifdef DIRECTIONALS
	for(i = 0; i < numDirLights; i++)
		output.color.rgb += DoDirLight(
			lights[i + firstDirLight], normal) * surfDiffuse;
#endif
#ifdef POINTLIGHTS
	for(i = 0; i < numPointLights; i++)
		output.color.rgb += DoPointLight(
			lights[i + firstPointLight], vertex, normal) * surfDiffuse;
#endif
#ifdef SPOTLIGHTS
	for(i = 0; i < numSpotLights; i++)
		output.color.rgb += DoSpotLight(
			lights[i + firstSpotLight], vertex, normal) * surfDiffuse;
#endif

	output.color = clamp(output.color, 0.0f, 1.0f);
	output.color *= matCol;
	output.texcoord = input.texcoord;
	return output;
}
