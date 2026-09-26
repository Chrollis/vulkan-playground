#version 450

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D screenTexture;
layout(set = 0, binding = 1) uniform sampler2D hudTexture;

layout(push_constant) uniform HudPush {
    int showHud;
} hudPush;

vec3 draw_hud(vec3 color, vec2 pixel) {
    if (hudPush.showHud == 0) {
        return color;
    }

    vec2 resolution = vec2(textureSize(screenTexture, 0));
    const vec2 hud_size = vec2(256.0, 160.0);
    const float margin = 16.0;
    vec2 origin = vec2(resolution.x - hud_size.x - margin, margin);

    if (any(lessThan(pixel, origin)) ||
        any(greaterThanEqual(pixel, origin + hud_size))) {
        return color;
    }

    vec2 uv = (pixel - origin) / hud_size;
    color = mix(color, vec3(0.04, 0.05, 0.08), 0.65);
    float mask = texture(hudTexture, uv).r;
    return mix(color, vec3(0.92), mask);
}

vec3 bilateral_denoise(sampler2D tex, vec2 uv) {
    vec2 texel = 1.0 / vec2(textureSize(tex, 0));
    vec3 center = texture(tex, uv).rgb;

    vec3 sum = center;
    float weight_sum = 1.0;

    const float sigma_color = 0.08;
    const float sigma_space = 1.0;

    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            if (x == 0 && y == 0) {
                continue;
            }

            vec3 sample_color = texture(tex, uv + vec2(x, y) * texel).rgb;
            vec3 diff = sample_color - center;
            float weight = exp(-dot(diff, diff) /
                               (2.0 * sigma_color * sigma_color));
            weight *= exp(-float(x * x + y * y) /
                          (2.0 * sigma_space * sigma_space));

            sum += sample_color * weight;
            weight_sum += weight;
        }
    }

    return sum / weight_sum;
}

void main() {
    vec3 color = bilateral_denoise(screenTexture, vUV);
    color = draw_hud(color, gl_FragCoord.xy);
    outColor = vec4(color, 1.0);
}
