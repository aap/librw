cbuffer Im2DConstants : register(b0)
{
    float4 xform;
};

struct VSIn
{
    float4 position : POSITION;
    float4 color : COLOR0;
    float2 texcoord : TEXCOORD0;
};

struct VSOut
{
    float4 position : SV_POSITION;
    float4 color : COLOR0;
    float2 texcoord : TEXCOORD0;
};

VSOut main(VSIn input)
{
    VSOut output;
    float z = input.position.w;
    float4 position = input.position;
    position.xy = position.xy * xform.xy + xform.zw;
    position.xyz *= z;
    position.w = z;
    output.position = position;
    output.color = float4(input.color.z, input.color.y, input.color.x, input.color.w);
    output.texcoord = input.texcoord;
    return output;
}
