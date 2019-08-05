#version 410

in vec3 WorldPos;
in vec3 Normal;
in vec2 TexCoord;

out vec4 frag_color;

// material parameters
uniform vec3 u_albedo;
uniform float u_metallic;
uniform float u_roughness;
uniform float u_ao;

uniform vec3 u_light_pos[4];
uniform vec3 u_light_color[4];

uniform vec3 u_cam_pos;

const float PI = 3.14159265359;

vec3 fresnel_schlick(float cos_theta, vec3 F0) {
	return F0 + (1.0 - F0) * pow(1.0 - cos_theta, 5.0);
}

float chi_ggx(float v) {
	return v > 0 ? 1 : 0;
}

float ndf_ggx(vec3 N, vec3 H, float roughness) {
	float NdH = dot(N,H);
	float alpha_2 = roughness * roughness;
	float NdH_2 = NdH * NdH;
	float denom = NdH_2 * alpha_2 + (1 - NdH_2);
	return (chi_ggx(NdH) * alpha_2) / ( PI * denom * denom );
}


float geometry_shlick_ggx(vec3 N, vec3 V, float roughness) {
	float r = (roughness + 1.0);
	float k = (r*r)/8.0;
	float numer = max(dot(N,V), 0.0);
	float denom = numer * (1.0 - k) + k;
	return numer / denom;
}

float geometry_smith(vec3 N, vec3 V, vec3 L, float roughness) {
	float ggx_1 = geometry_shlick_ggx(N, V, roughness);
	float ggx_2 = geometry_shlick_ggx(N, L, roughness);
	return ggx_1 * ggx_2;
}

void main() {
	vec3 N = vec3(Normal);
	vec3 V = normalize(u_cam_pos - WorldPos);
	vec3 F0 = vec3(0.04); // diaelectric
	F0 = mix(F0, u_albedo, u_metallic);

	// Reflectance equation
	vec3 L_o = vec3(0.0);
	for (int i = 0; i < 4; i++) {
		vec3 L = normalize(u_light_pos[i] - WorldPos);
		vec3 H = normalize(V + L);
		float distance = length(u_light_pos[i] - WorldPos);
		float attenuation = 1.0/(distance * distance);
		vec3 radiance = u_light_color[i] * attenuation;
		// Cook-Torrance BRDF
		float NDF = ndf_ggx(N, H, u_roughness);
		float G = geometry_smith(N, V, L, u_roughness);
		vec3 F = fresnel_schlick(clamp(dot(H,V), 0.0, 1.0), F0);
		vec3 numer = NDF * G * F;
		float denom = 4 * max(dot(N,V), 0.0) * max(dot(N,L), 0.0) + 0.001;
		vec3 specular = numer/max(denom, 0.001);
		vec3 ks = F;
		vec3 kd = vec3(1.0) - ks;
		// Only non-metals have diffuse lighting
		kd *= 1.0 - u_metallic;
		float NdL = max(dot(N,L),0.0);
		L_o += (kd * u_albedo/PI + specular) * radiance * NdL;
	}

	vec3 ambient = vec3(0.03) * u_albedo * u_ao;
	vec3 color = ambient + L_o;

	// HDR tonemapping
	color = color / (color + vec3(1.0));
	// gamma correct
	color = pow(color, vec3(1.0/2.2));

	frag_color = vec4(color, 1.0);
}
