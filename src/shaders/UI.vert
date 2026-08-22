#version 410

in vec3 vPosition;   // unit quad, [-1, 1] both axes, y up
in vec2 texCoord;

out vec2 TexCoord;

/* Button rect in window content pixels, y-down from the top-left — the
   same space glfwGetCursorPos reports, so hit testing is a direct
   comparison. uViewport is the GL viewport these NDCs scale into (window
   size; on HiDPI the framebuffer, a uniform scale of window space). */
uniform vec4 uRect;
uniform vec2 uViewport;

void main()
{
    vec2 px = uRect.xy
        + vec2(vPosition.x * 0.5 + 0.5, 0.5 - vPosition.y * 0.5) * uRect.zw;
    vec2 ndc = vec2(px.x * 2.0 / uViewport.x - 1.0,
                    1.0 - px.y * 2.0 / uViewport.y);
    gl_Position = vec4(ndc, 0.0, 1.0);
    TexCoord = texCoord;
}
