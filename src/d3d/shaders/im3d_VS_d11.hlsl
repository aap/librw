// Shared by the D3D11 Im3D path through im3DTransform. This covers
// immediate-mode 3D primitives such as textured view planes, camera frusta
// and debug lines; regular Atomic geometry and Im2D use different shaders.
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

    // D3D9 and D3D11 use different pixel-center conventions. In the camera
    // example, opaque texels in the Im3D view plane write depth before its
    // coplanar outline is drawn, so the coverage difference can hide parts
    // of that outline. Apply the half-pixel correction only to Im3D; applying
    // it to the global viewport would also shift Im2D and blur ImGui text.
    output.position.x += output.position.w * viewportOffset.x;
    output.position.y -= output.position.w * viewportOffset.y;
    output.color = float4(input.color.z, input.color.y, input.color.x, input.color.w);
    output.texcoord = input.texcoord;
    return output;
}
