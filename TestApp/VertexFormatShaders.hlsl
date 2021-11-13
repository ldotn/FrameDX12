struct InstanceData
{
	float4x4 World;
	float4x4 WVP;
};

StructuredBuffer<InstanceData> InstancesData;

struct StandardVertex
{
	float3 pos : POSITION;
	float3 normal : NORMAL;
	float4 tangent : TANGENT;
	float2 uv : UV;
	uint instance_id : SV_InstanceID;
};

struct AxisAngleVertex
{
	float3 pos : Position;
	float3 axis : Axis;
	snorm half2 angle_cos_sin : Angle;
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

PSIn VS_Standard(StandardVertex input)
{
	PSIn output = (PSIn)0;

	InstanceData data = InstancesData[input.instance_id];

	output.spos = mul(float4(input.pos, 1.0f), data.WVP);
	output.normal = mul(input.normal, (float3x3)data.World);
	output.tangent = mul(input.tangent.xyz, (float3x3)data.World);
	output.bitangent = input.tangent.w * cross(output.normal, output.tangent);

	return output;
}

float3 RodriguesRotation(float3 vec,float2 cos_sin, float3 axis)
{
	return vec * cos_sin.x + cos_sin.y * cross(axis, vec) + (1 - cos_sin.x) * dot(axis, vec) * vec;
}

PSIn VS_AxisAngle(AxisAngleVertex input)
{
	PSIn output = (PSIn)0;

	InstanceData data = InstancesData[input.instance_id];

	output.spos = mul(float4(input.pos, 1.0f), data.WVP);

	// Rodrigues' rotation formula
	float3 normal    = RodriguesRotation(float3(0, 0, 1), input.angle_cos_sin, input.axis);
	float3 tangent   = RodriguesRotation(float3(0, 1, 0), input.angle_cos_sin, input.axis);
	float3 bitangent = RodriguesRotation(float3(1, 0, 0), input.angle_cos_sin, input.axis);

	output.normal    = mul(normal, (float3x3)data.World);
	output.tangent   = mul(tangent, (float3x3)data.World);
	output.bitangent = mul(bitangent, (float3x3)data.World);

	return output;
}

float4 PSMain(PSIn input) : SV_Target0
{
	float3 n = normalize(input.normal);
	float3 t = normalize(input.tangent);
	float3 b = normalize(input.bitangent);

	return float4(b, 1.0f);
	//return float4(normalize(n + t + b) * 0.5 + 0.5, 1.0f);
}