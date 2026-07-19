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

#define surfAmbient (surfProps.x)
#define surfSpecular (surfProps.y)
#define surfDiffuse (surfProps.z)

#define fogStart (fogData.x)
#define fogEnd (fogData.y)
#define fogRange (fogData.z)
#define fogDisable (fogData.w)

#define numDirLights (numLights.x)
#define numPointLights (numLights.y)
#define numSpotLights (numLights.z)

#define firstDirLight (firstLight.x)
#define firstPointLight (firstLight.y)
#define firstSpotLight (firstLight.z)

struct VSInput
{
	float3 Position : POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR0;
    float2 texcoord : TEXCOORD0;
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

	output.Position = mul(combinedMat, float4(input.Position, 1.0f));
    output.color = input.color * matCol;
    output.texcoord = input.texcoord;
    return output;
}
