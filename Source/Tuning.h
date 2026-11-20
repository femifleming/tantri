#pragma once
// Tantri tuning: 22 shrutis (just ratios), 12 swara classes each with one or two shruti
// candidates, and a table of ragas. Nothing here is 12-TET: every pitch is Sa (Hz) x ratio.
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>

namespace tantri
{
struct Shruti { const char* label; int num, den; };

inline constexpr std::array<Shruti, 22> kShruti {{
    { "Sa", 1, 1 },      { "re", 256, 243 }, { "re", 16, 15 },   { "Re", 10, 9 },   { "Re", 9, 8 },
    { "ga", 32, 27 },    { "ga", 6, 5 },     { "Ga", 5, 4 },     { "Ga", 81, 64 },  { "Ma", 4, 3 },
    { "Ma", 27, 20 },    { "Ma'", 45, 32 },  { "Ma'", 729, 512 },{ "Pa", 3, 2 },    { "dha", 128, 81 },
    { "dha", 8, 5 },     { "Dha", 5, 3 },    { "Dha", 27, 16 },  { "ni", 16, 9 },   { "ni", 9, 5 },
    { "Ni", 15, 8 },     { "Ni", 243, 128 } }};

// swara class -> its two shruti candidates (Sa and Pa are fixed)
inline constexpr int kSwaraShruti[12][2] = { { 0, 0 }, { 1, 2 }, { 3, 4 }, { 5, 6 }, { 7, 8 }, { 9, 10 },
                                             { 11, 12 }, { 13, 13 }, { 14, 15 }, { 16, 17 }, { 18, 19 }, { 20, 21 } };
inline constexpr const char* kSwaraName[12] = { "Sa", "re", "Re", "ga", "Ga", "Ma", "Ma'", "Pa", "dha", "Dha", "ni", "Ni" };

inline double shrutiRatio (int s)  { return double (kShruti[(size_t) s].num) / kShruti[(size_t) s].den; }
inline double shrutiCents (int s)  { return 1200.0 * std::log2 (shrutiRatio (s)); }
inline int shrutiToSwara (int s)
{
    for (int w = 0; w < 12; ++w)
        if (kSwaraShruti[w][0] == s || kSwaraShruti[w][1] == s) return w;
    return 0;
}

// Packed tuning: bits 0-11 swaras in the raga, 16-27 second-candidate picks, 32-43 andolan swaras.
struct TuningBits
{
    uint64_t v = 0;
    bool in (int w) const      { return (v >> w) & 1; }
    int pick (int w) const     { return int ((v >> (16 + w)) & 1); }
    bool andolan (int w) const { return (v >> (32 + w)) & 1; }
    void setIn (int w, bool b)      { v = b ? v | (1ull << w) : v & ~(1ull << w); }
    void setPick (int w, int p)     { v = p ? v | (1ull << (16 + w)) : v & ~(1ull << (16 + w)); }
    void setAndolan (int w, bool b) { v = b ? v | (1ull << (32 + w)) : v & ~(1ull << (32 + w)); }
    int shruti (int w) const   { return kSwaraShruti[w][pick (w)]; }
    double ratio (int w) const { return shrutiRatio (shruti (w)); }
    int count() const          { int n = 0; for (int w = 0; w < 12; ++w) n += in (w); return n; }
};

struct Raga { const char* name; const char* spec; };
// spec tokens: S, r1 r2 R1 R2 g1 g2 G1 G2 m1 m2 (shuddha Ma) M1 M2 (tivra Ma'), P, d1 d2 D1 D2 n1 n2 N1 N2;
// the digit picks the shruti candidate, '~' marks andolan.
inline constexpr std::array<Raga, 25> kRagas {{
    { "Yaman",            "S R2 G1 M1 P D2 N1" },
    { "Bhairav",          "S r1~ G1 m1 P d1~ N1" },
    { "Bhairavi",         "S r2 g2 m1 P d2 n2" },
    { "Todi",             "S r1 g1 M2 P d1 N2" },
    { "Kafi",             "S R2 g2 m1 P D1 n2" },
    { "Khamaj",           "S R2 G1 m1 P D1 n1 N1" },
    { "Bilawal",          "S R2 G1 m1 P D1 N1" },
    { "Asavari",          "S R2 g2 m1 P d2 n2" },
    { "Marwa",            "S r2 G1 M2 D1 N2" },
    { "Purvi",            "S r2 G1 m1 M1 P d2 N1" },
    { "Darbari Kanada",   "S R2 g1~ m1 P d1~ n1" },
    { "Malkauns",         "S g2 m1 d2 n1" },
    { "Bhupali",          "S R2 G1 P D1" },
    { "Durga",            "S R2 m1 P D1" },
    { "Desh",             "S R2 G1 m1 P D1 n1 N1" },
    { "Bageshri",         "S R2 g2 m1 P D1 n1" },
    { "Puriya Dhanashri", "S r2 G1 M1 P d2 N1" },
    { "Shree",            "S r1~ G1 M1 P d2 N1" },
    { "Bihag",            "S R2 G1 m1 M1 P D1 N1" },
    { "Jog",              "S g2 G1 m1 P n1" },
    { "Kirwani",          "S R2 g2 m1 P d2 N1" },
    { "Chandrakauns",     "S g2 m1 d2 N1" },
    { "Hamsadhwani",      "S R2 G1 P N1" },
    { "Megh",             "S R2 m1 P n1" },
    { "Sarva (all 12)",   "S r2 R2 g2 G1 m1 M1 P d2 D1 n1 N1" } }};

inline TuningBits parseRaga (const char* spec)
{
    TuningBits t;
    // default candidates when a swara is not named: Re 9/8, Dha 5/3, others first
    t.setPick (2, 1);
    std::string s (spec);
    size_t i = 0;
    while (i < s.size())
    {
        while (i < s.size() && s[i] == ' ') ++i;
        if (i >= s.size()) break;
        char c = s[i++];
        int digit = 1;
        if (i < s.size() && (s[i] == '1' || s[i] == '2')) digit = s[i++] - '0';
        bool andolan = false;
        if (i < s.size() && s[i] == '~') { andolan = true; ++i; }
        int w = -1;
        switch (c)
        {
            case 'S': w = 0; break;  case 'r': w = 1; break;  case 'R': w = 2; break;
            case 'g': w = 3; break;  case 'G': w = 4; break;  case 'm': w = 5; break;
            case 'M': w = 6; break;  case 'P': w = 7; break;  case 'd': w = 8; break;
            case 'D': w = 9; break;  case 'n': w = 10; break; case 'N': w = 11; break;
            default: break;
        }
        if (w < 0) continue;
        t.setIn (w, true);
        t.setPick (w, digit - 1);
        t.setAndolan (w, andolan);
    }
    return t;
}

// nearest raga swara to swara class w (ties go down, as a sitarist would fall back to the lower fret)
inline int snapToRaga (const TuningBits& t, int w)
{
    if (t.in (w)) return w;
    for (int d = 1; d < 12; ++d)
    {
        if (t.in ((w - d + 12) % 12)) return w - d;   // may be negative: caller handles octave
        if (t.in ((w + d) % 12)) return w + d;
    }
    return 0;
}

// next raga swara above class w (for meend / gamak targets), returned as ratio relative to w's ratio
inline double nextSwaraRatio (const TuningBits& t, int w)
{
    for (int d = 1; d <= 12; ++d)
    {
        int c = w + d;
        int oct = c / 12;
        int cl = c % 12;
        if (t.in (cl)) return (t.ratio (cl) * std::pow (2.0, oct)) / t.ratio (w);
    }
    return 2.0;
}
} // namespace tantri
