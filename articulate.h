#ifndef OP1F_GARDEN_ARTICULATE_H
#define OP1F_GARDEN_ARTICULATE_H
#include <stdint.h>
#define ART_BEAT 26460000u
#define ART_KINDS 12u
#define ART_POLLEN 12u
#define ART_CHORD_MAX 8u
#define ART_LINE_MAX 26u
#define ART_NONE 255u
#define ART_PHRASES 16u
#define ART_DIVS 12u
enum { ART_S_CHORD, ART_S_STRUM, ART_S_STRUM2, ART_S_SLOP, ART_S_ARP, ART_S_FARP, ART_S_PATTERN,
       ART_S_HARP, ART_STYLES };
enum { ART_O_UP, ART_O_DOWN, ART_O_UPDOWN, ART_O_DOWNUP, ART_O_INSIDE, ART_O_RANDOM, ART_ORDERS };
enum { ART_V_CLOSE, ART_V_DROP2, ART_V_DROP3, ART_V_OPEN, ART_V_SHELL, ART_V_SPREAD, ART_VOICINGS };
extern const uint8_t art_kind_tones[ART_KINDS][4];
extern const uint8_t art_kind_count[ART_KINDS];
extern const uint8_t art_pollen_interval[ART_POLLEN];
extern const char* const art_kind_names[ART_KINDS];
extern const char* const art_pollen_names[ART_POLLEN];
extern const char* const art_style_names[ART_STYLES];
extern const char* const art_order_names[ART_ORDERS];
extern const char* const art_voicing_names[ART_VOICINGS];
extern const char* const art_phrase_names[ART_PHRASES];
uint32_t art_div(uint32_t d);
int art_pollen_silent(uint32_t q, uint32_t e);
uint32_t art_chord(uint32_t q, uint32_t mask, uint8_t out[ART_CHORD_MAX]);
uint32_t art_base(int32_t root, uint32_t q, uint32_t mask, uint8_t out[ART_CHORD_MAX]);
uint32_t art_cascade(const uint8_t* base, uint32_t n, int32_t k, uint8_t out[ART_CHORD_MAX]);
uint32_t art_voicing(uint32_t v, uint8_t* notes, uint32_t n);
int32_t art_lead(const uint8_t* prev, uint32_t nprev, const uint8_t* base, uint32_t n, uint32_t voicing, int32_t limit, int32_t cur);
uint32_t art_voice(const uint8_t* base, uint32_t n, int32_t k, uint32_t voicing, uint8_t out[ART_CHORD_MAX]);
static inline uint32_t art_style_for(uint32_t style, uint32_t mono) {
    return mono && (style == ART_S_CHORD || style == ART_S_HARP) ? (uint32_t)ART_S_STRUM : style;
}
static inline int art_stepped(uint32_t style) {
    return style == ART_S_ARP || style == ART_S_FARP || style == ART_S_PATTERN;
}
static inline int art_strummed(uint32_t style) {
    return style == ART_S_STRUM || style == ART_S_STRUM2 || style == ART_S_SLOP || style == ART_S_HARP;
}
typedef struct {
    const uint8_t* notes;
    uint32_t n;
    uint8_t style, order, phrase, mono;
    uint8_t loop, gate, human, pad_;
    int32_t boost;
    int32_t below;
    uint32_t step;
    uint32_t bar;
    int32_t velocity;
} art_req;
typedef struct {
    uint8_t note, source, pad_[2];
    uint32_t delay, gate;
    int32_t velocity;
} art_note;
uint32_t art_line(const art_req* r, uint8_t line[ART_LINE_MAX]);
uint32_t art_length(const art_req* r);
uint32_t art_period(const art_req* r);
uint32_t art_step(const art_req* r, uint32_t pos, uint32_t* rng, art_note out[ART_LINE_MAX]);
static inline uint32_t art_q16(uint32_t x, uint32_t q) {
    return (x >> 16) * q + (((x & 0xffffu) * q) >> 16);
}
static inline uint32_t art_pct(uint32_t pct) {
    return pct >= 100u ? 65536u : (pct * 671089u) >> 10;
}
static inline uint32_t art_rnd(uint32_t* s) {
    uint32_t x = *s ? *s : 0x9e3779b9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}
static inline uint32_t art_below(uint32_t* s, uint32_t n) {
    return ((art_rnd(s) >> 16) * n) >> 16;
}
#endif
