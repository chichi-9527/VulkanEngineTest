
struct UniformBufferObject
{
    float4x4 model;
    float4x4 view;
    float4x4 projection;
}

[[vk::location(0)]]
    ConstantBuffer<UniformBufferObject> ubo;

struct VSInput
{
    [[vk::location(0)]] float2 inPosition;
    [[vk::location(1)]] float3 inColor;
};

struct VSOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float3 fragColor;
};

struct PSInput
{
    [[vk::location(0)]] float3 fragColor;
};

struct PSOutput
{
    [[vk::location(0)]] float4 outColor;
};

[shader("vertex")]
VSOutput vsMain(VSInput input)
{
    VSOutput output;
    output.position = mul(float4(input.inPosition, 0.0f, 1.0f),mul(ubo.model,mul(ubo.view,ubo.projection)));
    output.fragColor = input.inColor;
    return output;
}

[shader("fragment")]
PSOutput psMain(PSInput input)
{
    PSOutput output;
    output.outColor = float4(input.fragColor, 1.0f);
    return output;
}