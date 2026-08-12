cbuffer ClearConstants : register(b0)
{
    float4 clearColor;
};

float4 main() : SV_Target
{
    return clearColor;
}
