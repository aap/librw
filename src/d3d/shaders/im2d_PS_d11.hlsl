Texture2D tex0 : register(t0);
SamplerState samp0 : register(s0);

#define ALPHA_ALWAYS 0
#define ALPHA_GREATEREQUAL 1
#define ALPHA_LESS 2

cbuffer AlphaTestConstants : register(b1)
{
    uint alphaEnabled;
    uint alphaFunction;
    float alphaReference;
    float padding;
};

struct PSIn
{
    float4 position : SV_POSITION;
    float4 color : COLOR0;
    float2 texcoord : TEXCOORD0;
};

float4 main(PSIn input) : SV_Target
{
    float4 color = input.color * tex0.Sample(samp0, input.texcoord);
    if(alphaEnabled != 0 &&
       ((alphaFunction == ALPHA_GREATEREQUAL && color.a < alphaReference) ||
        (alphaFunction == ALPHA_LESS && color.a >= alphaReference)))
        discard;
    return color;
}
