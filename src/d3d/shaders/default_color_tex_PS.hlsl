struct VS_out {
	float4 Position		: POSITION;
	float2 TexCoord0	: TEXCOORD0;
	float4 Color		: COLOR0;
};

sampler2D tex0 : register(s0);


float4 main(VS_out input) : COLOR
{
	return tex2D(tex0, input.TexCoord0.xy) * input.Color;
}
