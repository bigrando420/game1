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

struct PointLight {
	float2 position;
	float2 _pad1;
	float4 color;
	float radius;
	float intensity;
	float2 _pad3;
};

static const int LIGHT_MAX = 256;

cbuffer some_cbuffer : register(b0) {
	float night_alpha;
	float3 _pad;
	PointLight point_lights[LIGHT_MAX];
	int point_light_count;
}

float4 pixel_shader_extension(PS_INPUT input, float4 color) {

	// Store the original color before night shading
	float3 original_color = color.rgb;

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

	// Apply point lights with smooth accumulation
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
				float attenuation = smoothstep(light.radius, 0.0, distance);

				// Multiply attenuation by light intensity (from alpha channel)
				float light_intensity = light.color.a * light.intensity;
				float weighted_attenuation = attenuation * light_intensity;

				// Accumulate total illumination
				total_illumination += weighted_attenuation;

				// Accumulate weighted light colors
				accumulated_light_color += light.color.rgb * weighted_attenuation;
				accumulated_alpha += weighted_attenuation;
			}
		}

		// Cap total_illumination to [0, 1]
		total_illumination = saturate(total_illumination);

		// Normalize accumulated light color
		if (accumulated_alpha > 0.0) {
			accumulated_light_color /= accumulated_alpha;
		}

		// Blend between night-shaded and original color based on total illumination
		float3 lit_color = lerp(night_shaded_color, original_color, total_illumination);

		// Apply averaged light color tint to the lit color
		float constant_to_reduce_light_brightness = 0.5;
		lit_color = lerp(lit_color, accumulated_light_color, total_illumination * constant_to_reduce_light_brightness);

		// Set the final color
		color.rgb = lit_color;
	}

	// Clamp the final color
	color.rgb = saturate(color.rgb);

	return color;
}