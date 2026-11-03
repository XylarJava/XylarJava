#version 120

uniform sampler2D DiffuseSampler;
uniform sampler2D PrevSampler;

varying vec2 texCoord;

// How much of the previous frame stays: 0 = none, 0.9 = long trails.
uniform float BlendFactor;

void main() {
    vec4 current = texture2D(DiffuseSampler, texCoord);
    vec4 previous = texture2D(PrevSampler, texCoord);
    gl_FragColor = vec4(mix(current.rgb, previous.rgb, BlendFactor), 1.0);
}
