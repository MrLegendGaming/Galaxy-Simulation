#version 330 core
out vec4 FragColor;

// simple, angle-independent glow around the projected circle

uniform vec3  color;          // glow colour, e.g. vec3(1.0)
uniform float glowStrength;   // intensity, e.g. 3.0

// from C++ Step 1
uniform vec2  centerScreen;   // center of sphere in pixels
uniform float sphereRadiusPx; // radius of the solid sphere on screen (pixels)
uniform float glowWidthPx;    // thickness of glow band (pixels)

uniform float time;

void main()
{
    // fragment position in screen space
    vec2 frag = gl_FragCoord.xy;

    // distance from sphere centre
    float dist = length(frag - centerScreen);

    float inner = sphereRadiusPx;                 // where the solid sphere ends
    float outer = sphereRadiusPx + glowWidthPx;   // where glow ends

    // how far outside the sphere edge we are: 0 at edge, 1 at outer border
    float d = (dist - inner) / (outer - inner);
    d = clamp(d, 0.0, 9000.0);

    // smooth falloff (0 at outer edge, 1 at sphere edge)
    float falloff = exp(-time * 0.1 * d * d);   // tweak 3.0 sharper/softer

    vec3 glow = color * glowStrength * falloff;

    // alpha = falloff so blending can fade it out
    FragColor = vec4(glow, falloff);
}
