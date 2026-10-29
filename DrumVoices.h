#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

// Analog modellierte Drum-Stimmen: Kick, Snare, Closed/Open Hi-Hat, Clap, Rimshot.
// Reine DSP-Klassen ohne JUCE-Abhängigkeit.
namespace drums
{
constexpr float kPi    = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;

// Zeit bis -60 dB
inline float expCoef (float seconds, float sr) { return std::exp (-6.9078f / (std::max (seconds, 1.0e-4f) * sr)); }

inline float polyBlep (float t, float dt)
{
    if (t < dt)        { t /= dt;             return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

struct Noise
{
    uint32_t s = 22222u;
    float next()
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float) (int32_t) s * (1.0f / 2147483648.0f);
    }
};

// Zustandsvariablen-Filter (TPT)
struct SVF
{
    float ic1 = 0, ic2 = 0, a1 = 0, a2 = 0, a3 = 0, k = 1;

    void set (float fc, float q, float sr)
    {
        fc = std::min (std::max (fc, 20.0f), sr * 0.45f);
        const float g = std::tan (kPi * fc / sr);
        k = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void reset() { ic1 = ic2 = 0; }

    inline void tick (float v0, float& lp, float& bp, float& hp)
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1; hp = v0 - k * v1 - v2;
    }
};

// ------------------------------------------------------------------ Bassdrum
// Sinus mit schneller Pitch-Hüllkurve, Klick-Transient und Sättigung.
struct Kick
{
    struct Params { float tune = 52.f, decay = 0.55f, punch = 0.55f, click = 0.35f, drive = 0.25f; } p;
    float sr = 44100.f;
    bool active = false;
    float phase = 0, amp = 0, pitchEnv = 0, clickEnv = 0, vel = 1;
    float baseF = 50, startF = 300, ampC = 0, pitchC = 0, clickC = 0;
    Noise noise;
    SVF clickHp;

    void prepare (double s) { sr = (float) s; active = false; }

    void trigger (float v)
    {
        vel = v; active = true; phase = 0; amp = 1; pitchEnv = 1; clickEnv = 1;
        baseF = p.tune;
        startF = baseF * (1.0f + p.punch * 6.0f);
        ampC = expCoef (p.decay, sr);
        pitchC = expCoef (0.045f, sr);
        clickC = expCoef (0.004f, sr);
        clickHp.set (2500.0f, 0.7f, sr);
        clickHp.reset();
    }

    float process()
    {
        if (! active) return 0.0f;
        pitchEnv *= pitchC;
        const float f = baseF + (startF - baseF) * pitchEnv;
        phase += f / sr;
        if (phase >= 1.0f) phase -= 1.0f;
        amp *= ampC;
        clickEnv *= clickC;

        float lp, bp, hp;
        clickHp.tick (noise.next(), lp, bp, hp);

        float out = std::sin (kTwoPi * phase) * amp + hp * clickEnv * p.click * 0.6f;
        const float g = 1.0f + p.drive * 5.0f;
        out = std::tanh (out * g) / std::tanh (g);

        if (amp < 1.0e-4f) active = false;
        return out * vel;
    }
};

// ------------------------------------------------------------------ Snare
// Zwei Sinus-Oszillatoren (Body) plus hochpassgefiltertes Rauschen (Snappy).
struct Snare
{
    struct Params { float tune = 190.f, tone = 0.5f, snappy = 0.65f, decay = 0.22f; } p;
    float sr = 44100.f;
    bool active = false;
    float ph1 = 0, ph2 = 0, bodyEnv = 0, noiseEnv = 0, pitchEnv = 0, vel = 1;
    float bodyC = 0, noiseC = 0, pitchC = 0;
    Noise noise;
    SVF hp, lpf;

    void prepare (double s) { sr = (float) s; active = false; }

    void trigger (float v)
    {
        vel = v; active = true; ph1 = ph2 = 0; bodyEnv = noiseEnv = pitchEnv = 1;
        bodyC  = expCoef (0.05f + p.decay * 0.35f, sr);
        noiseC = expCoef (p.decay, sr);
        pitchC = expCoef (0.02f, sr);
        hp.set (1500.0f + p.tone * 2500.0f, 0.8f, sr);
        lpf.set (6000.0f + p.tone * 3000.0f, 0.7f, sr);
        hp.reset(); lpf.reset();
    }

    float process()
    {
        if (! active) return 0.0f;
        pitchEnv *= pitchC; bodyEnv *= bodyC; noiseEnv *= noiseC;

        const float f1 = p.tune * (1.0f + 0.5f * pitchEnv);
        const float f2 = f1 * 1.87f;
        ph1 += f1 / sr; if (ph1 >= 1.0f) ph1 -= 1.0f;
        ph2 += f2 / sr; if (ph2 >= 1.0f) ph2 -= 1.0f;
        const float body = (std::sin (kTwoPi * ph1) * 0.65f + std::sin (kTwoPi * ph2) * 0.35f) * bodyEnv;

        float lp, bp, h, lp2, bp2, h2;
        hp.tick (noise.next(), lp, bp, h);
        lpf.tick (h, lp2, bp2, h2);
        const float snap = lp2 * noiseEnv * p.snappy;

        if (bodyEnv < 1.0e-4f && noiseEnv < 1.0e-4f) active = false;
        return (body * 0.8f + snap * 0.7f) * vel;
    }
};

// ------------------------------------------------------------------ Hi-Hat
// Sechs Rechteck-Oszillatoren mit metallischen Frequenzen, Band- und Hochpass.
struct Hat
{
    struct Params { float tune = 1.0f, decay = 0.06f, tone = 0.5f; } p;
    float sr = 44100.f;
    bool active = false;
    float ph[6] = { 0, 0, 0, 0, 0, 0 };
    float amp = 0, ampC = 0, vel = 1, gain = 5.5f;
    SVF bp, hp;

    static constexpr float kFreq[6] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };

    void prepare (double s) { sr = (float) s; active = false; }

    void trigger (float v)
    {
        vel = v; active = true; amp = 1;
        for (auto& x : ph) x = 0;
        ampC = expCoef (p.decay, sr);
        bp.set (7000.0f + p.tone * 3500.0f, 1.2f, sr);
        hp.set (5500.0f + p.tone * 3500.0f, 0.7f, sr);
        bp.reset(); hp.reset();
    }

    // Closed Hat würgt die Open Hat ab
    void choke() { if (active) ampC = expCoef (0.008f, sr); }

    float process()
    {
        if (! active) return 0.0f;
        float sum = 0;
        for (int i = 0; i < 6; ++i)
        {
            const float dt = kFreq[i] * p.tune / sr;
            ph[i] += dt; if (ph[i] >= 1.0f) ph[i] -= 1.0f;
            float sq = ph[i] < 0.5f ? 1.0f : -1.0f;
            sq += polyBlep (ph[i], dt);
            float q = ph[i] + 0.5f; if (q >= 1.0f) q -= 1.0f;
            sq -= polyBlep (q, dt);
            sum += sq;
        }
        sum *= 0.18f;

        float lp, b, h, lp2, bp2, h2;
        bp.tick (sum, lp, b, h);
        hp.tick (b * bp.k, lp2, bp2, h2);

        amp *= ampC;
        if (amp < 1.0e-4f) active = false;
        return h2 * amp * gain * vel;
    }
};

// ------------------------------------------------------------------ Clap
// Gefiltertes Rauschen mit vier schnellen Bursts und anschließendem Ausklang.
struct Clap
{
    struct Params { float tone = 1100.f, decay = 0.3f, spread = 0.011f; } p;
    float sr = 44100.f;
    bool active = false;
    float env = 0, fastC = 0, slowC = 0, vel = 1;
    int counter = 0, spacing = 0, burst = 0;
    Noise noise;
    SVF bp;

    void prepare (double s) { sr = (float) s; active = false; }

    void trigger (float v)
    {
        vel = v; active = true; env = 0; counter = 0; burst = 0;
        spacing = std::max (1, (int) (p.spread * sr));
        fastC = expCoef (0.012f, sr);
        slowC = expCoef (p.decay, sr);
        bp.set (p.tone, 1.6f, sr);
        bp.reset();
    }

    float process()
    {
        if (! active) return 0.0f;
        if (burst < 4 && counter >= burst * spacing) { env = 1.0f; ++burst; }
        env *= (burst < 4) ? fastC : slowC;
        ++counter;

        float lp, b, h;
        bp.tick (noise.next(), lp, b, h);

        if (burst >= 4 && env < 1.0e-4f) active = false;
        return b * bp.k * env * 2.4f * vel;
    }
};

// ------------------------------------------------------------------ Rimshot
// Zwei Sinus-Töne (Obertonverhältnis ~3.4), hart gesättigt, plus kurzer Klick.
struct Rim
{
    struct Params { float tune = 520.f, decay = 0.035f; } p;
    float sr = 44100.f;
    bool active = false;
    float ph1 = 0, ph2 = 0, env = 0, envC = 0, clickEnv = 0, clickC = 0, vel = 1;
    Noise noise;
    SVF hp;

    void prepare (double s) { sr = (float) s; active = false; }

    void trigger (float v)
    {
        vel = v; active = true; ph1 = ph2 = 0; env = 1; clickEnv = 1;
        envC = expCoef (p.decay, sr);
        clickC = expCoef (0.0025f, sr);
        hp.set (2000.0f, 0.7f, sr);
        hp.reset();
    }

    float process()
    {
        if (! active) return 0.0f;
        env *= envC; clickEnv *= clickC;
        ph1 += p.tune / sr;        if (ph1 >= 1.0f) ph1 -= 1.0f;
        ph2 += p.tune * 3.4f / sr; if (ph2 >= 1.0f) ph2 -= 1.0f;

        const float tone = std::tanh (1.8f * (std::sin (kTwoPi * ph1) * 0.7f + std::sin (kTwoPi * ph2) * 0.5f)) * env;
        float lp, bp, h;
        hp.tick (noise.next(), lp, bp, h);

        if (env < 1.0e-4f && clickEnv < 1.0e-4f) active = false;
        return (tone * 0.8f + h * clickEnv * 0.4f) * vel;
    }
};
}
