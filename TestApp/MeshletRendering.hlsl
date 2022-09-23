struct Vertex
{
	float3 position;
	float3 normal;
	float4 tangent;
    float3 bitangent;
	float2 uv;

    uint _padding0;
};

struct Meshlet
{
    // The index for the first vertex of the meshlet
    uint start_vertex;
    // The number of triangles on the meshlet (ideally it will be 32 most of the time)
    uint triangles_num;
    // Number of vertices for the meshlet
    uint vertices_num;
    // The index for the first triangle of the meshlet
    uint start_triangle;
};

struct PSIn
{
    float4 spos : SV_Position;
    float3 normal : TEXCOORD0;
    float3 tangent : TEXCOORD1;
    float3 bitangent : TEXCOORD2;
    float3 meshlet_color : TEXCOORD3;
};

StructuredBuffer<Vertex>  Vertices  : register(t0);
StructuredBuffer<Meshlet> Meshlets  : register(t1);
ByteAddressBuffer         Triangles : register(t2);
ByteAddressBuffer         VertexIB  : register(t3);

cbuffer ConstantBuffer : register(b4)
{
    float4x4 World;
    float4x4 WVP;
};

// https://gist.github.com/iUltimateLP/5129149bf82757b31542
float3 hsv_to_rgb(float3 rgb)
{
    float4 K = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    float3 p = abs(frac(rgb.xxx + K.xyz) * 6.0 - K.www);

    return rgb.z * lerp(K.xxx, saturate(p - K.xxx), rgb.y);
}

float3 index_to_color(uint index)
{
    // LCG
    index = 1664525 * index + 1013904223;
    float3 color_lut[] =
    {
        float3(1,0,0), float3(0,1,0), float3(0,0,1),
        float3(1,1,0), float3(1,1,0), float3(0.75,0,1),
        float3(1,0,1), float3(0,1,1), float3(0,1,1),
        float3(1,1,1), float3(0,0,0), float3(0.5,1,0.5),
        float3(0.25,0.25,0.25), float3(0.5,0.5,0.5), float3(0.75,0.75,0.75)
    };
    //return color_lut[index % 15];
    return hsv_to_rgb(float3((index & 127) / 128.0f, 1, (((index >> 7) & 3) + 1) / 5.0f));
}

uint3 UnpackTriangle(uint trig)
{
    return uint3(trig & 1023, (trig >> 10) & 1023, (trig >> 20) & 1023);
}

[NumThreads(128, 1, 1)]
[OutputTopology("triangle")]
void MSMain(
    uint gtid : SV_GroupThreadID,
    uint gid : SV_GroupID,
    out indices uint3 tris[126],
    out vertices PSIn verts[64]
)
{
    // This is the same across the entire group so it should be broadcasted
    Meshlet meshlet = Meshlets[gid];
    SetMeshOutputCounts(meshlet.vertices_num, meshlet.triangles_num);

    if (gtid < meshlet.triangles_num)
    {
        uint packed_trig = Triangles.Load(4 * (meshlet.start_triangle + gtid));
        tris[gtid] = UnpackTriangle(packed_trig);
    }

    if (gtid < meshlet.vertices_num)
    {
        uint vid = VertexIB.Load(4 * (meshlet.start_vertex + gtid));
        Vertex v = Vertices[vid];

        PSIn output = (PSIn)0;
        output.spos = mul(float4(v.position, 1.0f), WVP);
        output.normal = mul(v.normal, (float3x3)World);
        output.meshlet_color = index_to_color(gid);
        verts[gtid] = output;
    }
}

float4 PSMain(PSIn input) : SV_Target0
{
    float3 n = normalize(input.normal);

    //return float4(n * 0.5 + 0.5, 1.0f);
    return float4(input.meshlet_color, 1.0f);
}