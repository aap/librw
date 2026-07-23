cbuffer Im3DConstants : register(b0)
{
    float4x4 combined;
    float4x4 world;
    float4x4 normal;
    float4 viewportOffset;
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
    output.position = mul(combined, float4(input.position, 1.0f));
    output.position.x += output.position.w * viewportOffset.x;
    output.position.y -= output.position.w * viewportOffset.y;
    output.color = float4(input.color.z, input.color.y, input.color.x, input.color.w);
    output.texcoord = input.texcoord;
    return output;
}
