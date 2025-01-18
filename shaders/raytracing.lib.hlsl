struct Payload
{
    float3 color;
    bool allowReflection;
    bool missed;
};
struct FrameCBuffer
{
    matrix invView;
    matrix invProjection;
};
struct FrameIndex
{
    uint frameIndex;
};

[[vk::push_constant]] ConstantBuffer<FrameIndex> frameIndex : register(b1);
[[vk::binding(0,0)]] ConstantBuffer<FrameCBuffer> camera : register(b0);
[[vk::binding(0,1)]] RWTexture2D<float4> image[] : register(u0, space1);
[[vk::binding(0,2)]] RaytracingAccelerationStructure scene[] : register(t0, space2);
[[vk::binding(0,3)]] Texture2D textures[] : register(t0, space3);
[[vk::binding(0,4)]] SamplerState samplers[] : register(s0, space4);

static const float3 light = float3(0, 200, 0);
static const float3 skyTop = float3(0.24, 0.44, 0.72);
static const float3 skyBottom = float3(0.75, 0.86, 0.93);


[shader("raygeneration")]
void RayGeneration()
{
    uint3 LaunchID = DispatchRaysIndex();
    uint3 LaunchSize = DispatchRaysDimensions();

    const float2 pixelCenter = float2(LaunchID.xy) + float2(0.5, 0.5);
    const float2 inUV = pixelCenter / float2(LaunchSize.xy);
    float2 d = inUV * 2.0 - 1.0;
    float4 target = mul(camera.invProjection, float4(d.x, d.y, 1, 1));

    RayDesc rayDesc;
    rayDesc.Origin = mul(camera.invView, float4(0, 0, 0, 1)).xyz;
    rayDesc.Direction = mul(camera.invView, float4(normalize(target.xyz), 0)).xyz;    
    rayDesc.TMin = 0.01;
    rayDesc.TMax = 1000.0;
    
    Payload payload;
    TraceRay(scene[0], RAY_FLAG_NONE, 0xff, 0, 0, 0, rayDesc, payload);

    image[frameIndex.frameIndex][int2(LaunchID.xy)] = float4(payload.color, 1.0);
}

[shader("miss")]
void Miss(inout Payload payload)
{

    float slope = normalize(WorldRayDirection()).y;
    float t = saturate(slope * 5 + 0.5);
    payload.color = lerp(skyBottom, skyTop, t);

    payload.missed = true;
}

[shader("closesthit")]
void ClosestHit(inout Payload payload,
                BuiltInTriangleIntersectionAttributes attrib)
{
    payload.color = float3(1, 1, 0);
}