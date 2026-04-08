OF_GLSL_SHADER_HEADER

#define MAX_LAYERS 15

uniform sampler2D uTextures[MAX_LAYERS];
uniform sampler2D uAccumTex;
uniform float uOpacities[MAX_LAYERS];
uniform int uBlendModes[MAX_LAYERS];
uniform int uNumLayers;
uniform int uHasAccum;

in vec2 vTexCoord;
out vec4 fragColor;

vec2 transformUV(vec2 uv, int i) {
    return uv;
}

vec4 adjustLayer(vec4 layer, int i) {
    return layer;
}

vec4 blend(vec4 acc, vec4 layer, float opacity, int mode) {
    if (mode == 0) {
        return mix(acc, layer, opacity);
    }
    else if (mode == 1) {
        return acc + layer * opacity;
    }
    else if (mode == 2) {
        if (length(acc.rgb) < 0.01) {
            return layer * opacity;
        }
        return acc * (layer + 1.0 - opacity);
    }
    else if (mode == 3) {
        vec4 screened = 1.0 - (1.0 - acc) * (1.0 - layer);
        return mix(acc, screened, opacity);
    }
    return acc;
}

void main() {
    vec4 acc = vec4(0.0);

    if (uHasAccum == 1) {
        acc = texture(uAccumTex, vTexCoord);
    }

    for (int i = 0; i < uNumLayers; i++) {
        vec2 uv = transformUV(vTexCoord, i);
        vec4 layer = texture(uTextures[i], uv);
        layer = adjustLayer(layer, i);
        acc = blend(acc, layer, uOpacities[i], uBlendModes[i]);
    }

    acc.a = 1.0;
    fragColor = acc;
}
