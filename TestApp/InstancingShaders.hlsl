struct InstanceData
{
	float4x4 World;
	float4x4 WVP;
};

StructuredBuffer<InstanceData> InstancesData;

struct VSIn
{
	float3 pos : POSITION;
	float3 normal : NORMAL;
	float4 tangent : TANGENT;
	float2 uv : UV;
	uint instance_id : SV_InstanceID;
};

struct PSIn
{
	float4 spos : SV_Position;
	float3 normal : TEXCOORD0;
	float3 tangent : TEXCOORD1;
	float3 bitangent : TEXCOORD2;
};

PSIn VSMain(VSIn input)
{
	PSIn output = (PSIn)0;

	InstanceData data = InstancesData[input.instance_id];

	output.spos = mul(float4(input.pos, 1.0f), data.WVP);
	output.normal = mul(input.normal, (float3x3)data.World);
	output.tangent = mul(input.tangent.xyz, (float3x3)data.World);
	output.bitangent = input.tangent.w * cross(output.normal, output.tangent);

	return output;
}

float4 PSMain(PSIn input) : SV_Target0
{
	float3 n = normalize(input.normal);
	float3 t = normalize(input.tangent);
	float3 b = normalize(input.bitangent);

	return float4(n * 0.5 + 0.5, 1.0f);
}