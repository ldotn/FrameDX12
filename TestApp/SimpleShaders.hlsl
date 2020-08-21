cbuffer ConstantBuffer : register(b0)
{
	float4x4 World;
	float4x4 WVP;
};

struct VSIn
{
	float3 pos : POSITION;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float2 uv : UV;
};

struct PSIn
{
	float4 spos : SV_Position;
	float3 normal : TEXCOORD0;
};

PSIn VSMain(VSIn input)
{
	PSIn output = (PSIn)0;

	output.spos = mul(float4(input.pos, 1.0f), WVP);

	output.normal = mul(input.normal, (float3x3)World);
	return output;
}

float4 PSMain(PSIn input) : SV_Target0
{
	float3 n = normalize(input.normal);

	return float4(n*0.5+0.5, 1.0f);
}