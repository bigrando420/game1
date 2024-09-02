/*
// copied from gfx_impl_d3d11.c
struct PS_INPUT
{
	float4 position_screen : SV_POSITION;
	float4 position : POSITION;
	float2 uv : TEXCOORD0;
	float2 self_uv : SELF_UV;
	float4 color : COLOR;
	int texture_index: TEXTURE_INDEX;
	int type: TYPE;
	int sampler_index: SAMPLER_INDEX;
	uint has_scissor : HAS_SCISSOR;
	float4 userdata[$VERTEX_2D_USER_DATA_COUNT] : USERDATA;
	float4 scissor : SCISSOR;
};
*/

cbuffer some_cbuffer : register(b0) {
	float4 col_override;
}

float4 pixel_shader_extension(PS_INPUT input, float4 color) {
	float4 col_override = input.userdata[0];
	color.rgb = lerp(color.rgb, col_override.rgb, col_override.a);
	return color;
}