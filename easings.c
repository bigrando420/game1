// https://easings.net/
// not comprehesive. I just write them as needed.

// all designed to operate on alpha values, 0.0 -> 1.0

float ease_out_sin(float alpha) {
	return sinf((alpha * PI32) / 2.0f);
}

float ease_out_circ(float alpha) {
	return sqrtf(1.0f - powf(alpha - 1.0f, 2.0f));
}

float ease_parabolic(float alpha) {
	return 1.0f - powf(alpha * 2.0f - 1.0f, 2.0f);
}

float ease_in_back(float alpha) {
	float c1 = 1.70158f;
	float c3 = c1 + 1.0f;
	return c3 * alpha * alpha * alpha - c1 * alpha * alpha;
}

float ease_out_back(float alpha, float multiplier) {
	float c1 = 1.70158f * multiplier;
	float c3 = c1 + 1.0f;
	return 1.0f + c3 * powf(alpha - 1.0f, 3.0f) + c1 * powf(alpha - 1.0f, 2.0f);
}

float ease_out_elastic(float alpha) {
	float c4 = (2.0f * PI32) / 3.0f;
	if (alpha == 0.0f) return 0.0f;
	if (alpha == 1.0f) return 1.0f;
	return powf(2.0f, -10.0f * alpha) * sinf((alpha * 10.0f - 0.75f) * c4) + 1.0f;
}

float ease_sin_breathe(float alpha) {
	return (sinf((alpha - 0.25f) * 2.0f * PI32) / 2.0f) + 0.5f;
}

float ease_in_out_cubic(float alpha) {
	if (alpha < 0.5f) {
			return 4.0f * alpha * alpha * alpha;
	} else {
			return 1.0f - powf(-2.0f * alpha + 2.0f, 3.0f) / 2.0f;
	}
}

float ease_in_cubic(float alpha) {
	return alpha * alpha * alpha;
}

float ease_out_exp(float alpha, float rate) {
	if (alpha == 1.0f) return 1.0f;
	return 1.0f - powf(2.0f, -rate * alpha);
}

float ease_in_exp(float alpha, float rate) {
	if (alpha == 0.0f) return 0.0f;
	return powf(2.0f, rate * alpha - rate);
}