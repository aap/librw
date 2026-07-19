cbuffer StandardConstants : register(b0)
{
    row_major float4x4 combinedMat;
	row_major float4x4 worldMat;
	row_major float4x4 normalMat;
	float4 matCol;
	float4 surfProps;
	float4 fogData;
	float4 ambientLight;
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

	output.Position = mul(float4(input.Position, 1.0f), combinedMat);
    output.color = input.color * matCol;
    output.texcoord = input.texcoord;
    return output;
}
