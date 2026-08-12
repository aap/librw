float4 main(uint vertexID : SV_VertexID) : SV_POSITION
{
    float2 position;
    position.x = (vertexID == 1) ? 3.0f : -1.0f;
    position.y = (vertexID == 2) ? -3.0f : 1.0f;
    return float4(position, 1.0f, 1.0f);
}
