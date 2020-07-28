struct VSIn
{
	float3 pos : POSITION;
};

struct PSIn
{
	float4 spos : SV_Position;
};

PSIn VSMain(VSIn input)
{
	PSIn output = (PSIn)0;

	output.spos = float4(input.pos, 1.0f);
	return output;
}

float4 PSMain(PSIn input) : SV_Target0
{
	return float4(input.spos.xyz, 1.0f);
}