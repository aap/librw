Texture2D diffTex : register(t0);
Texture2D envTex : register(t1);
SamplerState diffSampler : register(s0);
SamplerState envSampler : register(s1);

#define ALPHA_ALWAYS 0
#define ALPHA_GREATEREQUAL 1
#define ALPHA_LESS 2

cbuffer D3D9StateConstants : register(b2)
{
	uint alphaEnabled;
	uint alphaFunction;
	float alphaReference;
	float alphaPadding;
};

cbuffer MatFXConstants : register(b3)
{
	float shininess;
	float disableFBA;
	float2 matFXPadding;
};

struct PSInput
{
	float4 position : SV_POSITION;
	float2 texcoord0 : TEXCOORD0;
	float2 texcoord1 : TEXCOORD1;
	float fog : TEXCOORD2;
	float4 color : COLOR0;
	float4 envColor : COLOR1;
};

float4 main(PSInput input) : SV_Target
{
	float4 pass1 = input.color;
#ifdef TEX
	pass1 *= diffTex.Sample(diffSampler, input.texcoord0);
#endif

	float4 pass2 = input.envColor * shininess *
		envTex.Sample(envSampler, input.texcoord1);

	pass2.rgb = lerp(float3(0.0f, 0.0f, 0.0f), pass2.rgb, input.fog);

	float fba = max(pass1.a, disableFBA);
	float4 color;
	color.rgb = pass1.rgb * pass1.a + pass2.rgb * fba;
	color.a = pass1.a;

	if(alphaEnabled != 0 &&
	   ((alphaFunction == ALPHA_GREATEREQUAL && color.a < alphaReference) ||
	    (alphaFunction == ALPHA_LESS && color.a >= alphaReference)))
		discard;
	return color;
}
