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

/*
	Samplers:
	
	image_sampler_0 // near POINT,  far POINT
	image_sampler_1 // near LINEAR, far LINEAR
	image_sampler_2 // near POINT,  far LINEAR
	image_sampler_3 // near LINEAR, far POINT

*/

/*

userdata layout
[0] = col override
[1].x = 32 bits of flags

*/

#define QUAD_TYPE_portal 1

struct PointLight {
	float2 position;
	float radius;
	float intensity;
	float4 color;
};

static const int LIGHT_MAX = 256;

cbuffer some_cbuffer : register(b0) {
	float night_alpha;
	float3 _pad;
	PointLight point_lights[LIGHT_MAX];
	int point_light_count;
}

Texture2D portal_tex: register(t0);

float4 pixel_shader_extension(PS_INPUT input, float4 color) {

	// Store the original color before night shading
	float3 original_color = color.rgb;

	float type = input.userdata[1].x;
	if (type == QUAD_TYPE_portal) {

		float3 mask_col = float3(239.0 / 255.0, 106.0 / 255.0, 216.0 / 255.0);
		if (all(abs(original_color - mask_col) < 0.01)) {
			// sample into portal tex
			float2 uv = input.uv;
			uv.y = 1.0-uv.y;
			original_color = portal_tex.Sample(image_sampler_0, uv).xyz;

			// I think we can just return dis bc lighting was already done?
			return float4(original_color.xyz, 1);
		}
	}

	// Override color
	float4 col_override = input.userdata[0];
	original_color = lerp(original_color, col_override.rgb, col_override.a);

	// Apply night shade
	{
		// Desaturate the color
		float luminance = dot(original_color, float3(0.299, 0.587, 0.114));
		float3 grayscale = float3(luminance, luminance, luminance);
		color.rgb = lerp(original_color, grayscale, night_alpha * 0.2);

		// Darken the color
		float darken_factor = 1.0 - (night_alpha * 0.7);
		color.rgb *= darken_factor;

		// Shift the color towards blue
		float3 blue_tint = float3(0.0, 0.0, 1.0);
		float blue_shift_amount = night_alpha * 0.01;
		color.rgb = lerp(color.rgb, lerp(color.rgb, blue_tint, blue_shift_amount), night_alpha);
	}

	// Store the night-shaded color
	float3 night_shaded_color = color.rgb;

	// Combined lighting pass
	{
		float total_illumination = 0.0;
		float3 accumulated_light_color = float3(0.0, 0.0, 0.0);
		float accumulated_alpha = 0.0;
		float2 pixel_pos = input.position_screen.xy; // Screen space coordinates

		// Loop over point lights
		for (int i = 0; i < LIGHT_MAX; ++i) {
			PointLight light = point_lights[i];

			// Compute vector from light to pixel in screen space
			float2 light_dir = pixel_pos - light.position;
			float distance = length(light_dir);

			// Check if within light radius
			if (distance < light.radius) {
				// Calculate attenuation (using smoothstep for smoother falloff)
				float attenuation = smoothstep(light.radius, 0.0, distance) * light.intensity;

				// Accumulate total illumination
				total_illumination += attenuation;

				// If the light has color (alpha > 0), accumulate the light color
				if (light.color.a > 0.0) {
					float color_alpha = light.color.a * attenuation;
					accumulated_light_color += light.color.rgb * color_alpha;
					accumulated_alpha += color_alpha;
				}
			}
		}

		// Clamp total_illumination to [0, 1]
		total_illumination = saturate(total_illumination);

		// Blend between night-shaded and original color based on total illumination
		float3 lit_color = lerp(night_shaded_color, original_color, total_illumination);

		// Apply accumulated light color tint if any
		if (accumulated_alpha > 0.0) {
			// Normalize accumulated light color
			accumulated_light_color /= accumulated_alpha;

			// Apply color tint to the lit color
			lit_color = lerp(lit_color, accumulated_light_color, accumulated_alpha);
		}

		// Set the final color
		color.rgb = lit_color;
	}

	// Clamp the final color
	color.rgb = saturate(color.rgb);

	return color;
}