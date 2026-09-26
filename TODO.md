# TODO

## PBR / Lighting

- [x] Point light inverse-square distance attenuation
- [x] Neutral / adjustable point light colors
- [x] Colored shadows through transmissive materials
- [x] Proper BRDF LUT texture instead of analytic approximation
- [x] Unified iterative path tracing
- [x] Multi-bounce diffuse global illumination
- [x] Spot lights
- [x] Area lights
- [x] alphaMode / alphaCutoff / doubleSided
- [x] Metal GGX importance sampling
- [x] Firefly clamp
- [x] Environment importance sampling / MIS
- [x] Lightweight bilateral denoise
- [x] KHR_materials_emissive_strength
- [x] KHR_materials_specular factors
- [x] KHR_materials_clearcoat factors
- [x] KHR_texture_transform
- [x] Texture sRGB / mipmap / wrap mode
- [x] glTF sampler wrap / filter modes
- [x] Explicit texture LOD (ray cone + UV density)
- [x] Normal mapping from the hit triangle cotangent frame
- [ ] Split shaders/raytrace.comp below 800 lines with #include