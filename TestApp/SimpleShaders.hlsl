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

	output.spos = float4(input.pos*0.5, 1.0f);
	output.spos.x *= 9.0f / 16.0f;
	output.spos.z = (output.spos.z + 1.6) / 3.2;
	output.normal = input.normal;
	return output;
}

float4 PSMain(PSIn input) : SV_Target0
{
	return float4(input.normal, 1.0f);
}