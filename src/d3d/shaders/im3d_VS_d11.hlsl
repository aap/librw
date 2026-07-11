cbuffer Im3DConstants : register(b0)
{
    row_major float4x4 combined;
    row_major float4x4 world;
    row_major float4x4 normal;
};

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR0;
    float2 texcoord : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float4 color : COLOR0;
    float2 texcoord : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = mul(float4(input.position, 1.0f), combined);
    output.color = float4(input.color.z, input.color.y, input.color.x, input.color.w);
    output.texcoord = input.texcoord;
    return output;
}
