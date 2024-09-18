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

static const int LIGHT_MAX = 128;

cbuffer some_cbuffer : register(b0) {
	float night_alpha;
	float3 _pad;
	PointLight pointLights[LIGHT_MAX];
	int point_light_count;
}

float4 pixel_shader_extension(PS_INPUT input, float4 color) {

	// Store the original color before night shading
	float3 originalColor = color.rgb;

	// Override color
	float4 col_override = input.userdata[0];
	originalColor = lerp(originalColor, col_override.rgb, col_override.a);

	// Apply night shade
	{
		// Desaturate the color
		float luminance = dot(originalColor, float3(0.299, 0.587, 0.114));
		float3 grayscale = float3(luminance, luminance, luminance);
		color.rgb = lerp(originalColor, grayscale, night_alpha * 0.2);

		// Darken the color
		float darken_factor = 1.0 - (night_alpha * 0.7);
		color.rgb *= darken_factor;

		// Shift the color towards blue
		float3 blue_tint = float3(0.0, 0.0, 1.0);
		float blue_shift_amount = night_alpha * 0.01;
		color.rgb = lerp(color.rgb, lerp(color.rgb, blue_tint, blue_shift_amount), night_alpha);
	}

	// Store the night-shaded color
	float3 nightShadedColor = color.rgb;

	// Apply point lights
	{
		float totalIllumination = 0.0;
		float3 accumulatedLightColor = float3(0.0, 0.0, 0.0);
		float accumulatedLightColorAlpha = 0.0;
		float2 pixelPos = input.position_screen.xy; // Screen space coordinates

		// Loop over point lights
		for (int i = 0; i < LIGHT_MAX; ++i) {
			PointLight light = pointLights[i];

			// Compute vector from light to pixel in screen space
			float2 lightDir = pixelPos - light.position;
			float distance = length(lightDir);

			// Check if within light radius
			if (distance < light.radius) {
				// Calculate attenuation (linear falloff)
				float attenuation = (1.0 - (distance / light.radius)) * light.intensity;

				// Accumulate illumination
				totalIllumination += attenuation;

				// Accumulate weighted light colors
				accumulatedLightColor += light.color.rgb * attenuation;//??? * light.color.a;
				accumulatedLightColorAlpha += light.color.a * attenuation;
			}
		}

		// Clamp totalIllumination to [0, 1]
		totalIllumination = saturate(totalIllumination);

		// If there is illumination, normalize the accumulated light color
		if (totalIllumination > 0.0) {
			accumulatedLightColor /= totalIllumination;
		}

		// Blend between night-shaded and original color based on illumination
		float3 litColor = lerp(nightShadedColor, originalColor, totalIllumination);

		// Apply light color tint to the lit color
		litColor = lerp(litColor, accumulatedLightColor, totalIllumination * accumulatedLightColorAlpha);

		// Set the final color
		color.rgb = litColor;
	}

	// Clamp the final color
	color.rgb = saturate(color.rgb);

	return color;
}