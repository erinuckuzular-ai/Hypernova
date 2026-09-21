#pragma once

#include <juce_opengl/juce_opengl.h>
#include "Style.h"

// The living backdrop: a domain-warped nebula drifting slowly, two parallax star layers that twinkle,
// and a black hole with a spinning accretion disk behind the logo. Drawn by one fragment shader on the
// GPU so it costs the CPU almost nothing; the panels are smoked glass on top.
// CosmosFallback paints a still version on the CPU for snapshots and machines without OpenGL.
namespace ab::ui
{

struct CosmosState
{
    std::atomic<float> level { 0.0f };          // output loudness 0..1, makes the nebula and disk breathe
    std::atomic<float> holeX { 0 }, holeY { 0 }, holeR { 0 }; // black hole centre/radius in component pixels
    std::atomic<float> flare { 0.0f };          // one-shot burst when the logo is clicked, decays to 0
};

class CosmosRenderer : public juce::OpenGLRenderer
{
public:
    CosmosRenderer (juce::OpenGLContext& c, juce::Component& target, CosmosState& s) : context (c), component (target), state (s) {}

    bool isReady() const { return ready.load(); }

    void newOpenGLContextCreated() override
    {
        using namespace juce;
        shader = std::make_unique<OpenGLShaderProgram> (context);
        const bool ok = shader->addVertexShader (OpenGLHelpers::translateVertexShaderToV3 (vertexSource))
                     && shader->addFragmentShader (OpenGLHelpers::translateFragmentShaderToV3 (fragmentSource))
                     && shader->link();
        if (! ok) { shader.reset(); return; }

        const GLfloat quad[] = { -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f };
        gl::glGenBuffers (1, &vbo);
        gl::glBindBuffer (gl::GL_ARRAY_BUFFER, vbo);
        gl::glBufferData (gl::GL_ARRAY_BUFFER, sizeof (quad), quad, gl::GL_STATIC_DRAW);
        start = Time::getMillisecondCounterHiRes();
        ready = true;
    }

    void renderOpenGL() override
    {
        using namespace juce;
        const float scale = (float) context.getRenderingScale();
        const int w = roundToInt (scale * (float) component.getWidth()), h = roundToInt (scale * (float) component.getHeight());
        gl::glViewport (0, 0, w, h);
        if (shader == nullptr)
        {
            OpenGLHelpers::clear (Colours::bg0);
            return;
        }
        shader->use();
        const float t = (float) ((Time::getMillisecondCounterHiRes() - start) * 0.001);
        shader->setUniform ("resolution", (GLfloat) w, (GLfloat) h);
        shader->setUniform ("time", t);
        smoothedLevel += (state.level.load() - smoothedLevel) * 0.15f;
        shader->setUniform ("level", smoothedLevel);
        shader->setUniform ("flare", state.flare.load());
        shader->setUniform ("pixelScale", scale);
        shader->setUniform ("hole", state.holeX.load() * scale, (float) h - state.holeY.load() * scale, state.holeR.load() * scale);

        gl::glBindBuffer (gl::GL_ARRAY_BUFFER, vbo);
        const auto pos = (GLuint) gl::glGetAttribLocation (shader->getProgramID(), "position");
        gl::glEnableVertexAttribArray (pos);
        gl::glVertexAttribPointer (pos, 2, gl::GL_FLOAT, gl::GL_FALSE, 0, nullptr);
        gl::glDrawArrays (gl::GL_TRIANGLE_STRIP, 0, 4);
        gl::glDisableVertexAttribArray (pos);
        gl::glBindBuffer (gl::GL_ARRAY_BUFFER, 0);
    }

    void openGLContextClosing() override
    {
        ready = false;
        if (vbo != 0) juce::gl::glDeleteBuffers (1, &vbo);
        vbo = 0;
        shader.reset();
    }

private:
    juce::OpenGLContext& context;
    juce::Component& component;
    CosmosState& state;
    std::unique_ptr<juce::OpenGLShaderProgram> shader;
    GLuint vbo = 0;
    double start = 0;
    float smoothedLevel = 0;
    std::atomic<bool> ready { false };

    static constexpr const char* vertexSource = R"(
        attribute vec2 position;
        void main() { gl_Position = vec4 (position, 0.0, 1.0); }
    )";

    static constexpr const char* fragmentSource = R"(
        uniform vec2 resolution;
        uniform float time;
        uniform float level;
        uniform float flare;
        uniform float pixelScale;
        uniform vec3 hole;

        float hash (vec2 p) { p = fract (p * vec2 (123.34, 456.21)); p += dot (p, p + 45.32); return fract (p.x * p.y); }
        float noise (vec2 p)
        {
            vec2 i = floor (p), f = fract (p);
            f = f * f * (3.0 - 2.0 * f);
            return mix (mix (hash (i), hash (i + vec2 (1.0, 0.0)), f.x), mix (hash (i + vec2 (0.0, 1.0)), hash (i + vec2 (1.0, 1.0)), f.x), f.y);
        }
        float fbm (vec2 p)
        {
            float v = 0.0, a = 0.5;
            for (int i = 0; i < 5; i++) { v += a * noise (p); p = p * 2.03 + vec2 (17.1, 9.2); a *= 0.5; }
            return v;
        }

        void main()
        {
            float t = time;
            vec2 uv = gl_FragCoord.xy / resolution.y;
            vec3 violet = vec3 (0.34, 0.19, 0.86), magenta = vec3 (0.86, 0.17, 0.52), ion = vec3 (0.16, 0.74, 1.0), gold = vec3 (1.0, 0.63, 0.27);

            // Nebula: domain-warped fbm, drifting very slowly.
            vec2 q = uv * 1.5 + vec2 (t * 0.006, -t * 0.003);
            vec2 w = vec2 (fbm (q + vec2 (0.0, t * 0.012)), fbm (q + vec2 (5.2, 1.3) - t * 0.009));
            float n = fbm (q + 1.8 * w);
            float n2 = fbm (q * 2.2 - w + 11.0);
            vec3 col = vec3 (0.014, 0.016, 0.040);
            col += violet * smoothstep (0.30, 0.90, n) * 0.95;
            col += magenta * smoothstep (0.42, 0.95, n * w.x * 1.8) * 0.65;
            col += ion * smoothstep (0.55, 0.95, n2) * 0.40;
            col += gold * smoothstep (0.72, 1.0, n * n2 * 1.9) * 0.25;
            col *= 0.78 + 0.45 * level + 0.9 * flare;

            // Stars: two layers drifting at different speeds (parallax), twinkling.
            for (int L = 0; L < 2; L++)
            {
                float fl = float (L);
                float cell = (L == 0 ? 70.0 : 34.0) * pixelScale;
                vec2 sp = gl_FragCoord.xy / cell + vec2 (t * (0.015 + 0.02 * fl), t * 0.004);
                vec2 id = floor (sp), f = fract (sp) - 0.5;
                float h = hash (id + fl * 13.0);
                if (h > 0.82)
                {
                    vec2 off = vec2 (hash (id + 3.1), hash (id + 7.7)) - 0.5;
                    float d = length (f - off * 0.7) * cell / pixelScale;
                    float tw = 0.55 + 0.45 * sin (t * (0.8 + h * 3.0) + h * 60.0);
                    float size = L == 0 ? 1.6 : 0.9;
                    col += vec3 (0.82, 0.86, 1.0) * smoothstep (size, 0.0, d) * tw * (L == 0 ? 1.0 : 0.6);
                }
            }

            // Black hole with an accretion disk (tilted, spinning, brighter on the approaching side).
            if (hole.z > 0.0)
            {
                vec2 p = gl_FragCoord.xy - hole.xy;
                float R = hole.z;
                vec2 dp = vec2 (p.x, p.y * 3.0);
                float rd = length (dp) / R;
                float ang = atan (dp.y, dp.x);
                float disk = smoothstep (2.7, 1.35, rd) * smoothstep (0.95, 1.3, rd);
                float streak = 0.5 + 0.5 * sin (ang * 5.0 - t * 2.4 + rd * 7.0) * (0.6 + 0.4 * noise (vec2 (ang * 3.0 - t * 1.7, rd * 4.0)));
                float doppler = 0.6 + 0.4 * cos (ang - 0.4);
                vec3 diskCol = mix (gold, magenta, clamp (rd - 1.3, 0.0, 1.0));
                float r = length (p) / R;
                float front = p.y < 0.0 ? 1.0 : 0.0;
                vec3 glow = gold * exp (-max (r - 1.0, 0.0) * 1.2) * 0.22 * (1.0 + level + 6.0 * flare);
                col += glow;
                col += vec3 (1.0, 0.82, 0.58) * exp (-pow ((r - 1.06) * 9.0, 2.0)) * 0.95;           // photon ring
                col = mix (col, vec3 (0.0), smoothstep (1.02, 0.9, r));                               // event horizon
                col += diskCol * disk * streak * doppler * (1.05 + 0.7 * level + 3.0 * flare) * (front > 0.5 ? 1.0 : (r > 1.0 ? 1.0 : 0.25));
            }

            vec2 v = gl_FragCoord.xy / resolution - 0.5;
            col *= 1.0 - dot (v, v) * 0.85;
            gl_FragColor = vec4 (col, 1.0);
        }
    )";
};

// CPU stand-in for the shader: same palette and composition, drawn once and cached.
inline juce::Image renderCosmosFallback (int w, int h, float scale, juce::Point<float> hole, float holeR)
{
    juce::Image img (juce::Image::ARGB, juce::roundToInt ((float) w * scale), juce::roundToInt ((float) h * scale), false);
    juce::Graphics g (img);
    g.addTransform (juce::AffineTransform::scale (scale));
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (Colours::bg0);
    g.fillRect (r);
    auto bloom = [&] (juce::Colour c, float x, float y, float rad)
    {
        g.setGradientFill (juce::ColourGradient (c, x, y, c.withAlpha (0.0f), x + rad, y, true));
        g.fillRect (r);
    };
    bloom (juce::Colour (0x5a5a30dc), (float) w * 0.22f, (float) h * 0.28f, (float) w * 0.45f);
    bloom (juce::Colour (0x40dc2b85), (float) w * 0.62f, (float) h * 0.62f, (float) w * 0.38f);
    bloom (juce::Colour (0x2a29bdff), (float) w * 0.9f, (float) h * 0.2f, (float) w * 0.3f);
    bloom (juce::Colour (0x305a30dc), (float) w * 0.8f, (float) h * 0.9f, (float) w * 0.35f);
    juce::Random rnd (11);
    for (int i = 0; i < 420; ++i)
    {
        const float s = 0.5f + rnd.nextFloat() * 1.3f;
        g.setColour (juce::Colour (0xffd2dbff).withAlpha (0.12f + rnd.nextFloat() * 0.55f));
        g.fillEllipse (rnd.nextFloat() * (float) w, rnd.nextFloat() * (float) h, s, s);
    }
    // Black hole: glow, disk, horizon, photon ring.
    g.setGradientFill (juce::ColourGradient (juce::Colour (0x66ffa14a), hole.x, hole.y, juce::Colour (0x00ffa14a), hole.x + holeR * 3.2f, hole.y, true));
    g.fillEllipse (juce::Rectangle<float> (holeR * 6.4f, holeR * 6.4f).withCentre (hole));
    juce::Path disk;
    disk.addEllipse (juce::Rectangle<float> (holeR * 5.0f, holeR * 1.5f).withCentre (hole));
    g.setGradientFill (juce::ColourGradient (Colours::warm, hole.x - holeR * 2.5f, hole.y, Colours::plasma.withAlpha (0.6f), hole.x + holeR * 2.5f, hole.y, false));
    g.strokePath (disk, juce::PathStrokeType (holeR * 0.45f));
    g.setColour (juce::Colours::black);
    g.fillEllipse (juce::Rectangle<float> (holeR * 2.0f, holeR * 2.0f).withCentre (hole));
    g.setColour (juce::Colour (0xffffd29a));
    g.drawEllipse (juce::Rectangle<float> (holeR * 2.12f, holeR * 2.12f).withCentre (hole), holeR * 0.12f);
    juce::Path front;
    front.addCentredArc (hole.x, hole.y, holeR * 2.5f, holeR * 0.75f, 0.0f, juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi * 3.0f, true);
    g.setColour (Colours::warm);
    g.strokePath (front, juce::PathStrokeType (holeR * 0.4f));
    // Vignette
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, r.getCentreX(), r.getCentreY(),
                                             juce::Colours::black.withAlpha (0.55f), 0, 0, true));
    g.fillRect (r);
    return img;
}

} // namespace ab::ui
