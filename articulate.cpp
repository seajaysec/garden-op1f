#include "articulate.h"
const uint8_t art_kind_tones[ART_KINDS][4] = {
    {0, 4, 7, 0},
    {0, 3, 7, 0},
    {0, 5, 7, 0},
    {0, 3, 6, 0},
    {0, 4, 8, 0},
    {0, 5, 10, 0},
    {0, 2, 7, 0},
    {0, 4, 6, 0},
    {0, 7, 12, 0},
    {0, 7, 14, 0},
    {0, 2, 5, 7},
    {0, 1, 7, 0},
};
const uint8_t art_kind_count[ART_KINDS] = {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 3};
const char* const art_kind_names[ART_KINDS] = {"maj", "min", "sus4", "dim", "aug", "qrt", "sus2", "b5", "pow", "qnt", "s24", "phr"};
const uint8_t art_pollen_interval[ART_POLLEN] = {9, 10, 11, 14, 13, 15, 18, 21, 17, 20, 8, 2};
const char* const art_pollen_names[ART_POLLEN] = {"6", "-7", "+7", "9", "b9", "#9", "#11", "13", "11", "b13", "#5", "2"};
const char* const art_style_names[ART_STYLES] = {"chord", "strum", "strum 2", "slop", "arp", "farp", "pattern", "harp"};
const char* const art_order_names[ART_ORDERS] = {"up", "down", "updown", "downup", "inside", "random"};
const char* const art_voicing_names[ART_VOICINGS] = {"close", "drop 2", "drop 3", "open", "shell", "spread"};
static const uint8_t TWENTYFOURTHS[ART_DIVS] = {192, 96, 48, 32, 24, 16, 12, 8, 6, 4, 3, 2};
uint32_t art_div(uint32_t d) {
    return (uint32_t)TWENTYFOURTHS[d < ART_DIVS ? d : 8u] * (ART_BEAT / 24u);
}
typedef struct { uint8_t tone, weight; } step_t;
typedef struct { uint8_t at, len; } phrase_t;
const char* const art_phrase_names[ART_PHRASES] = {
    "trot", "seesaw", "rain", "tide", "ladder up", "ladder dn", "skipping", "knock",
    "lullaby", "hiccup", "swirl", "pebbles", "heartbeat", "twinkle", "tumble", "bloom"};
static const step_t STEPS[] = {
          {1, 4}, {3, 2}, {2, 3}, {3, 2},
        {1, 4}, {4, 2}, {2, 3}, {4, 2},
          {4, 3}, {3, 2}, {2, 2}, {1, 4}, {0, 0}, {2, 1},
          {1, 4}, {2, 2}, {3, 3}, {4, 3}, {3, 2}, {2, 2},
     {1, 4}, {2, 2}, {3, 3}, {4, 2}, {5, 3}, {6, 2},
     {6, 4}, {5, 2}, {4, 3}, {3, 2}, {2, 3}, {1, 2},
      {1, 4}, {0, 0}, {3, 2}, {2, 3}, {0, 0}, {4, 2},
         {1, 4}, {0, 0}, {0, 0}, {1, 2}, {3, 3}, {0, 0}, {2, 2}, {0, 0},
       {1, 4}, {3, 2}, {3, 1},
        {1, 4}, {1, 1}, {0, 0}, {3, 3}, {2, 2}, {2, 1}, {0, 0}, {4, 2},
         {1, 4}, {3, 2}, {2, 2}, {4, 3}, {3, 2}, {5, 2}, {4, 3}, {2, 2},
       {2, 3}, {0, 0}, {4, 2}, {1, 4}, {0, 0}, {3, 2}, {0, 0}, {1, 1},
     {1, 4}, {1, 2}, {0, 0}, {0, 0},
       {4, 3}, {0, 0}, {5, 2}, {0, 0}, {4, 2}, {3, 1}, {0, 0}, {1, 4},
        {5, 4}, {3, 2}, {4, 3}, {2, 2}, {3, 3}, {1, 2},
         {1, 4}, {2, 1}, {3, 2}, {4, 3}, {5, 4}, {0, 0}, {3, 2}, {0, 0},
};
static const phrase_t PHRASES[ART_PHRASES] = {
    {0, 4}, {4, 4}, {8, 6}, {14, 6}, {20, 6}, {26, 6}, {32, 6}, {38, 8},
    {46, 3}, {49, 8}, {57, 8}, {65, 8}, {73, 4}, {77, 8}, {85, 6}, {91, 8}};
static_assert(sizeof STEPS / sizeof STEPS[0] == 99u, "the phrase table and its index agree");
static const uint8_t WEIGHT16[5] = {0, 7, 10, 13, 16};
static uint32_t pitch_class(int32_t x) {
    while (x < 0) x += 12;
    while (x >= 12) x -= 12;
    return (uint32_t)x;
}
static int32_t in_range(int32_t note) {
    while (note < 0) note += 12;
    while (note > 127) note -= 12;
    return note;
}
static void ascending(uint8_t* a, uint32_t n) {
    for (uint32_t i = 1; i < n; ++i)
        for (uint32_t j = i; j > 0 && a[j] < a[j - 1u]; --j) {
            uint8_t t = a[j]; a[j] = a[j - 1u]; a[j - 1u] = t;
        }
}
static int holds(const uint8_t* a, uint32_t n, uint8_t x) {
    for (uint32_t i = 0; i < n; ++i) if (a[i] == x) return 1;
    return 0;
}
int art_pollen_silent(uint32_t q, uint32_t e) {
    if (q >= ART_KINDS || e >= ART_POLLEN) return 0;
    const uint32_t want = pitch_class(art_pollen_interval[e]);
    for (uint32_t i = 0; i < art_kind_count[q]; ++i)
        if (pitch_class(art_kind_tones[q][i]) == want) return 1;
    return 0;
}
uint32_t art_chord(uint32_t q, uint32_t mask, uint8_t out[ART_CHORD_MAX]) {
    if (q >= ART_KINDS) q = 0;
    uint32_t n = 0;
    for (uint32_t i = 0; i < art_kind_count[q]; ++i) out[n++] = art_kind_tones[q][i];
    for (uint32_t e = 0; e < ART_POLLEN && n < ART_CHORD_MAX; ++e)
        if (((mask >> e) & 1u) && !art_pollen_silent(q, e)) out[n++] = art_pollen_interval[e];
    ascending(out, n);
    return n;
}
uint32_t art_base(int32_t root, uint32_t q, uint32_t mask, uint8_t out[ART_CHORD_MAX]) {
    uint8_t steps[ART_CHORD_MAX];
    const uint32_t m = art_chord(q, mask, steps);
    uint32_t n = 0;
    for (uint32_t i = 0; i < m; ++i) {
        const uint8_t note = (uint8_t)in_range(root + steps[i]);
        if (!holds(out, n, note)) out[n++] = note;
    }
    ascending(out, n);
    return n;
}
uint32_t art_cascade(const uint8_t* base, uint32_t n, int32_t k, uint8_t out[ART_CHORD_MAX]) {
    if (n > ART_CHORD_MAX) n = ART_CHORD_MAX;
    for (uint32_t i = 0; i < n; ++i) out[i] = base[i];
    ascending(out, n);
    if (n == 0) return 0;
    while (k > 0 && out[0] <= 115) { out[0] = (uint8_t)(out[0] + 12); ascending(out, n); --k; }
    while (k < 0 && out[n - 1u] >= 12) { out[n - 1u] = (uint8_t)(out[n - 1u] - 12); ascending(out, n); ++k; }
    return n;
}
uint32_t art_voicing(uint32_t v, uint8_t* notes, uint32_t n) {
    if (n > ART_CHORD_MAX) n = ART_CHORD_MAX;
    int32_t x[ART_CHORD_MAX];
    uint32_t m = 0;
    for (uint32_t i = 0; i < n; ++i) {
        const uint32_t above = pitch_class((int32_t)notes[i] - (int32_t)notes[0]);
        if (v == ART_V_SHELL && n >= 4u && i > 0 && above >= 6u && above <= 8u) continue;
        x[m++] = notes[i];
    }
    switch (v) {
    case ART_V_DROP2: if (m >= 3u) x[m - 2u] -= 12; break;
    case ART_V_DROP3: if (m >= 4u) x[m - 3u] -= 12; break;
    case ART_V_OPEN: for (uint32_t i = 1; i < m; i += 2u) x[i] += 12; break;
    case ART_V_SPREAD: for (uint32_t i = 0; i < m; ++i) x[i] += 12 * (int32_t)(i >> 1); break;
    default: break;
    }
    uint32_t k = 0;
    for (uint32_t i = 0; i < m; ++i) {
        const uint8_t note = (uint8_t)in_range(x[i]);
        if (!holds(notes, k, note)) notes[k++] = note;
    }
    ascending(notes, k);
    return k;
}
uint32_t art_voice(const uint8_t* base, uint32_t n, int32_t k, uint32_t v, uint8_t out[ART_CHORD_MAX]) {
    const int32_t most = 2 * (int32_t)n;
    k = k > most ? most : k < -most ? -most : k;
    return art_voicing(v, out, art_cascade(base, n, k, out));
}
int32_t art_lead(const uint8_t* prev, uint32_t nprev, const uint8_t* base, uint32_t n, uint32_t v, int32_t limit, int32_t cur) {
    if (nprev == 0 || n == 0) return cur;
    int32_t best = cur, best_move = 0x7fffffff;
    for (int32_t k = -limit; k <= limit; ++k) {
        uint8_t t[ART_CHORD_MAX];
        const uint32_t m = art_voice(base, n, k, v, t);
        int32_t move = 0;
        for (uint32_t a = 0; a < m; ++a) {
            int32_t near = 0x7fff;
            for (uint32_t b = 0; b < nprev; ++b) {
                int32_t d = (int32_t)t[a] - (int32_t)prev[b];
                if (d < 0) d = -d;
                if (d < near) near = d;
            }
            move += near;
        }
        const int32_t size = k < 0 ? -k : k, best_size = best < 0 ? -best : best;
        if (move < best_move || (move == best_move && size < best_size)) { best_move = move; best = k; }
    }
    return best;
}
typedef struct { uint8_t pitch, from; } tone_t;
static uint32_t line_of(const art_req* r, tone_t line[ART_LINE_MAX]) {
    const uint32_t style = art_style_for(r->style, r->mono);
    const uint32_t octaves = style == ART_S_HARP ? 3u : (style == ART_S_STRUM2 || style == ART_S_FARP) ? 2u : 1u;
    const uint32_t count = r->n > ART_CHORD_MAX ? ART_CHORD_MAX : r->n;
    uint32_t n = 0;
    for (uint32_t o = 0; o < octaves; ++o)
        for (uint32_t i = 0; i < count && n + 1u < ART_LINE_MAX; ++i) {
            line[n].pitch = (uint8_t)in_range((int32_t)r->notes[i] + 12 * (int32_t)o);
            line[n].from = (uint8_t)i;
            ++n;
        }
    if (r->mono && r->boost >= 0 && n > 0 && n < ART_LINE_MAX) {
        tone_t b = { (uint8_t)r->boost, (uint8_t)ART_NONE };
        if (r->below) {
            for (uint32_t i = n; i > 0; --i) line[i] = line[i - 1u];
            line[0] = b;
        } else line[n] = b;
        ++n;
    }
    if (r->order == ART_O_INSIDE && style != ART_S_CHORD && style != ART_S_PATTERN && n > 1u) {
        tone_t t[ART_LINE_MAX];
        uint32_t lo = 0, hi = n - 1u;
        for (uint32_t k = 0; k < n; ++k) t[k] = (k & 1u) ? line[hi--] : line[lo++];
        for (uint32_t k = 0; k < n; ++k) line[k] = t[k];
    }
    return n;
}
uint32_t art_line(const art_req* r, uint8_t line[ART_LINE_MAX]) {
    tone_t t[ART_LINE_MAX];
    const uint32_t n = line_of(r, t);
    for (uint32_t i = 0; i < n; ++i) line[i] = t[i].pitch;
    return n;
}
static const phrase_t* phrase_of(const art_req* r) { return &PHRASES[r->phrase < ART_PHRASES ? r->phrase : 0u]; }
uint32_t art_length(const art_req* r) {
    const uint32_t style = art_style_for(r->style, r->mono);
    if (!art_stepped(style)) return 1u;
    if (style == ART_S_PATTERN) return phrase_of(r)->len;
    uint8_t line[ART_LINE_MAX];
    const uint32_t n = art_line(r, line);
    return (r->order == ART_O_UPDOWN || r->order == ART_O_DOWNUP) && n > 1u ? 2u * n - 2u : n;
}
uint32_t art_period(const art_req* r) {
    const uint32_t style = art_style_for(r->style, r->mono);
    if (art_stepped(style)) return r->step;
    if (!r->loop) return 0;
    return style == ART_S_CHORD ? r->step : r->bar;
}
static int32_t humanise(const art_req* r, int32_t vel, uint32_t* rng) {
    if (r->human == 0) return vel;
    const uint32_t amount = art_below(rng, 101u) * r->human;
    vel -= (int32_t)((amount * 1585u) >> 10);
    return vel < 5160 ? 5160 : vel;
}
uint32_t art_step(const art_req* r, uint32_t pos, uint32_t* rng, art_note out[ART_LINE_MAX]) {
    const uint32_t style = art_style_for(r->style, r->mono);
    tone_t line[ART_LINE_MAX];
    const uint32_t n = line_of(r, line);
    if (n == 0) return 0;
    const uint32_t length = art_length(r);
    if (!r->loop && pos >= length) return 0;
    const int stepped = art_stepped(style), strummed = art_strummed(style);
    uint32_t from = 0, to = n;
    int32_t velocity = r->velocity;
    if (stepped) {
        uint32_t at = pos;
        while (at >= length) at -= length;
        if (style == ART_S_PATTERN) {
            const step_t s = STEPS[phrase_of(r)->at + at];
            if (s.tone == 0) return 0;
            at = s.tone - 1u;
            while (at >= n) at -= n;
            velocity = (velocity * (int32_t)WEIGHT16[s.weight < 5u ? s.weight : 4u] + 8) >> 4;
            if (velocity < 1) velocity = 1;
        } else {
            if (at >= n) at = 2u * n - 2u - at;
            if (r->order == ART_O_DOWN || r->order == ART_O_DOWNUP) at = n - 1u - at;
            if (r->order == ART_O_RANDOM) at = art_below(rng, n);
        }
        from = at;
        to = at + 1u;
    } else if (style == ART_S_CHORD) to = r->n < n ? r->n : n;
    if (strummed) {
        if (r->order == ART_O_RANDOM)
            for (uint32_t i = n; i > 1u; --i) {
                const uint32_t j = art_below(rng, i);
                tone_t t = line[i - 1u]; line[i - 1u] = line[j]; line[j] = t;
            }
        int downward = r->order == ART_O_DOWN;
        if (r->order == ART_O_UPDOWN) downward = (int)(pos & 1u);
        if (r->order == ART_O_DOWNUP) downward = !(pos & 1u);
        if (downward)
            for (uint32_t i = 0, j = n - 1u; i < j; ++i, --j) { tone_t t = line[i]; line[i] = line[j]; line[j] = t; }
    }
    const int gated = stepped || style == ART_S_HARP || r->loop;
    const uint32_t gap = style == ART_S_HARP ? r->step >> 2 : r->step;
    const uint32_t span = (stepped || style == ART_S_CHORD) ? r->step : r->bar;
    uint32_t gate = gated ? art_q16(span, art_pct(r->gate)) : 0u;
    if (gated && gate == 0) gate = 1u;
    uint32_t k = 0, when = 0;
    for (uint32_t i = from; i < to; ++i, when += gap) {
        art_note* o = &out[k++];
        uint32_t delay = strummed ? when : 0u;
        if (style == ART_S_SLOP && i > 0) delay += art_q16(gap >> 1, art_rnd(rng) >> 16);
        o->note = line[i].pitch;
        o->source = line[i].from;
        o->pad_[0] = o->pad_[1] = 0;
        o->delay = delay;
        o->gate = gate;
        o->velocity = humanise(r, velocity, rng);
    }
    return k;
}
