struct VS_out {
	float4 Position		: POSITION;
	float2 TexCoord0	: TEXCOORD0;
	float4 Color		: COLOR0;
};

sampler2D tex0 : register(s0);


float4 main(VS_out input) : COLOR
{
	float4 color = input.Color;
#ifdef TEX
	color *= tex2D(tex0, input.TexCoord0.xy);
#endif
	return color;
}
