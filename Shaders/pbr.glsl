// PBR Reference
// https://google.github.io/filament/Filament.md.html

const float PI = 3.1415926f;

// Normal Distribution Function
// Approximates distribution of microfacet
float D_GGX(float NoH, float a) {
    float a2 = a * a;
    float f = (NoH * a2 - NoH) * NoH + 1.0;
    return a2 / (PI * f * f);
}

// Geometric Function models the visibility (or occlusion or shadow masking) of microfacets
// Smith Geometric Shadowing Function
// This is derived as G / (4 * (n.v) * (n.l)) from original equation. So we don't need to
// divide by the denominator while combining it all
float V_SmithGGXCorrelated(float NoV, float NoL, float roughness) {
    float a2 = roughness * roughness;
    float GGXV = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2);
    float GGXL = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);
    return 0.5 / (GGXV + GGXL);
}

// Approximation of V_SmithGGXCorrelated by removing square root and
// power
float V_SmithGGXCorrelatedFast(float NoV, float NoL, float roughness) {
    float a = roughness;
    float GGXV = NoL * (NoV * (1.0 - a) + a);
    float GGXL = NoV * (NoL * (1.0 - a) + a);
    return 0.5 / (GGXV + GGXL);
}

// Fresnel Schilick
vec3 F_Schlick(float LoH, vec3 f0) {
    return f0 + (1.0f - f0) * pow(1.0 - LoH, 5.0);
}
