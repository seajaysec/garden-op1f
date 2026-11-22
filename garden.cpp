#include "build_r6e_card.h"
#include "articulate.h"
#include "garden_parts.h"
typedef unsigned int u32;
#define SYNTH_SLOT (*(volatile u32*)0x11904704u)
#define SYNTH_ENVELOPES 0x08024ce8u
enum { TICKS_PER_BAR = 384 };
static inline u32 mask_off() { return op1f_mask(); }
static inline void mask_on(u32 prior) { op1f_unmask(prior); }
extern "C" const Op1fCard op1f_card_garden OP1F_NAMED("op1f_card_garden");
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static int mod12(int v) { while (v >= 12) v -= 12; while (v < 0) v += 12; return v; }
static int fold(int n) { while (n < 0) n += 12; while (n > 127) n -= 12; return n; }
static u32 udiv(u32 n, u32 d) {
    if (d == 0) return 0;
    u32 q = 0, r = 0;
    for (int i = 31; i >= 0; --i) {
        r = (r << 1) | ((n >> i) & 1u);
        if (r >= d) { r -= d; q |= 1u << i; }
    }
    return q;
}
static int sdiv(int n, int d) {
    int neg = (n < 0) != (d < 0);
    u32 q = udiv(n < 0 ? (u32)-n : (u32)n, d < 0 ? (u32)-d : (u32)d);
    return neg ? -(int)q : (int)q;
}
static u32 umod(u32 n, u32 d) { return n - udiv(n, d) * d; }
static u32 isqrt(u32 x) {
    u32 r = 0, bit = 1u << 30;
    while (bit > x) bit >>= 2;
    while (bit) {
        if (x >= r + bit) { x -= r + bit; r = (r >> 1) + bit; } else r >>= 1;
        bit >>= 2;
    }
    return r;
}
enum { STYLE_CHORD = 0, STYLE_STRUM, STYLE_STRUM2, STYLE_SLOP, STYLE_ARP, STYLE_FARP, STYLE_PATTERN, STYLE_HARP };
enum { NONE = 255, PULSE_RATES = 10, ECHO_RATES = 9, WALK_LENS = 4, WALK_ORDERS = 4, MOMENTARY_MS = 300 };
struct Settings {
    u32 magic, version;
    signed char cascade, boost;
    unsigned char style, rate, order, phrase, vtype, pulse;
    unsigned char loop, hold, lead, length, swing, human;
    unsigned char quality, fl, pf, pad_sel, square, walk_len, walk_order, root;
    unsigned short extensions;
    unsigned char echo, spare;
    unsigned char kinds[2][4], extsets[2][4];
    unsigned char seed_q[8], seed_ext[8][4], pad_root[8], pad_used[8];
};
enum { SAVE_MAGIC = 0x4e445247 , SAVE_VERSION = 6 };
static Settings g;
static volatile u32 edits;
static void defaults() {
    unsigned char* b = (unsigned char*)&g;
    for (u32 i = 0; i < sizeof g; ++i) b[i] = 0;
    g.magic = SAVE_MAGIC;
    g.version = SAVE_VERSION;
    g.style = STYLE_STRUM;
    g.rate = 8;
    g.length = 70;
    g.swing = 50;
    g.extensions = 1u << 2;
    g.root = 60;
    g.walk_len = 1;
    for (int r = 0; r < 2; ++r)
        for (int i = 0; i < 4; ++i) { g.kinds[r][i] = (unsigned char)(4 * r + i); g.extsets[r][i] = (unsigned char)(4 * r + i); }
    for (int i = 0; i < 8; ++i) {
        g.seed_q[i] = NONE;
        g.pad_root[i] = 60;
        for (int j = 0; j < 4; ++j) g.seed_ext[i][j] = NONE;
    }
}
static int settings_valid(const Settings* s) {
    if (s->magic != (u32)SAVE_MAGIC || s->version != (u32)SAVE_VERSION) return 0;
    if (s->cascade < -16 || s->cascade > 16 || s->boost < -2 || s->boost > 2 || s->style > 7 || s->rate > 11 ||
        s->order > 5 || s->phrase >= ART_PHRASES || s->vtype > 5 || s->pulse >= PULSE_RATES || s->loop > 1 || s->hold > 1 ||
        s->lead > 1 || s->length < 1 || s->length > 100 || s->swing < 50 || s->swing > 75 || s->human > 100 ||
        s->quality > 11 || s->fl > 1 || s->pf > 1 || s->pad_sel > 7 || s->square > 3 || s->walk_len >= WALK_LENS ||
        s->walk_order >= WALK_ORDERS || s->root > 127 || (s->extensions >> 12) || s->echo >= ECHO_RATES)
        return 0;
    for (int r = 0; r < 2; ++r)
        for (int i = 0; i < 4; ++i) if (s->kinds[r][i] > 11 || s->extsets[r][i] > 11) return 0;
    for (int i = 0; i < 8; ++i) {
        if (s->seed_q[i] != NONE && s->seed_q[i] > 11) return 0;
        if (s->pad_root[i] > 127 || s->pad_used[i] > 1) return 0;
        for (int j = 0; j < 4; ++j) if (s->seed_ext[i][j] != NONE && s->seed_ext[i][j] > 11) return 0;
    }
    return 1;
}
static u32 seed_mask(int i) {
    u32 m = 0;
    for (int j = 0; j < 4; ++j) if (g.seed_ext[i][j] < 12) m |= 1u << g.seed_ext[i][j];
    return m;
}
static int planted(int i) { return i >= 0 && i < 8 && g.pad_used[i] && g.seed_q[i] < 12; }
static u32 tones_of(u32 q, u32 mask) {
    u32 n = art_kind_count[q < 12 ? q : 0];
    for (u32 e = 0; e < 12; ++e) if (((mask >> e) & 1u) && !art_pollen_silent(q, e)) ++n;
    return n > 8 ? 8 : n;
}
enum { SRC_NONE, SRC_KEY, SRC_PAD, SRC_HELD, MAX_SCHED = 128, OUT_MAX = 160 };
enum { SYNTH_VOICES = 6, ECHO_GROUPS = 16, ECHO_REPEATS = 4 };
struct Sched { unsigned char note, vel, on, pad_; u32 due; };
static struct {
    unsigned char note[OUT_MAX], vel[OUT_MAX], on[OUT_MAX];
    u32 n;
} outbox;
static volatile u32 ons_sent, offs_sent, notes_refused;
static void flush() {
    unsigned char note[OUT_MAX], vel[OUT_MAX], on[OUT_MAX];
    u32 m = mask_off();
    u32 n = outbox.n;
    for (u32 i = 0; i < n; ++i) { note[i] = outbox.note[i]; vel[i] = outbox.vel[i]; on[i] = outbox.on[i]; }
    outbox.n = 0;
    mask_on(m);
    for (u32 i = 0; i < n; ++i) {
        if (on[i]) { if (op1f_note_on(&op1f_card_garden, note[i], vel[i], 0)) ons_sent = ons_sent + 1; else notes_refused = notes_refused + 1; }
        else if (op1f_note_off(&op1f_card_garden, note[i], 0)) offs_sent = offs_sent + 1; else notes_refused = notes_refused + 1;
    }
}
static struct {
    uint32_t rng;
    u32 now;
    int key_id;
    int src, latched;
    int root, vel, kind;
    u32 mask;
    int on;
    unsigned char voiced[ART_CHORD_MAX], prev[ART_CHORD_MAX];
    unsigned char nvoiced, nprev;
    int lead_k, lead_root, cascade_was;
    unsigned char count[128];
    u32 struck_at[128];
    Sched sched[MAX_SCHED];
    int nsched;
    u32 pos, period, anchor, pulse_next;
    int mono, pulse_note;
    int walk, walk_pad, walk_dir;
    u32 walk_at, walk_span;
    int planter;
} st;
static volatile struct {
    unsigned char note[ART_CHORD_MAX], n, playing, root, kind, boost_note, boosting, planter, walking, stepping, echoing;
    unsigned short mask;
    u32 pos, beat16, walk_q10;
} view;
static void emit(int note, int vel, int on) {
    if (outbox.n >= (u32)OUT_MAX) return;
    outbox.note[outbox.n] = (unsigned char)note;
    outbox.vel[outbox.n] = (unsigned char)vel;
    outbox.on[outbox.n] = (unsigned char)on;
    ++outbox.n;
}
static void note_on(int note, int vel) {
    if (note < 0 || note > 127) return;
    vel = clampi(vel, 1, 127);
    if (st.count[note] == 255) return;
    if (st.count[note]) emit(note, 64, 0);
    ++st.count[note];
    st.struck_at[note] = st.now;
    emit(note, vel, 1);
}
static void note_off(int note) {
    if (note < 0 || note > 127 || st.count[note] == 0) return;
    if (--st.count[note] == 0) emit(note, 64, 0);
}
static int pitches_sounding() {
    int n = 0;
    for (int i = 0; i < 128; ++i) n += st.count[i] != 0;
    return n;
}
static void let_go_now() {
    st.nsched = 0;
    for (int n = 0; n < 128; ++n)
        while (st.count[n]) note_off(n);
}
static u32 ticks_of(u32 units) {
    if (!units) return 0;
    u32 t = (art_q16(units, 15583u) + 32768u) >> 16;
    return t ? t : 1u;
}
static int play_mode_mono() {
    u32 slot = SYNTH_SLOT;
    if (slot >= 8u) return 0;
    u32 word = *(volatile u32*)(SYNTH_ENVELOPES + 32u * slot + 16u);
    return (word >> 28) != 0u;
}
static int boost_pitch() { return fold(st.root + 12 * g.boost); }
static void request_of(art_req* r, int velocity) {
    r->notes = st.voiced;
    r->n = st.nvoiced;
    r->style = g.style;
    r->order = g.order;
    r->phrase = g.phrase;
    r->mono = (uint8_t)st.mono;
    r->loop = g.loop;
    r->gate = g.length;
    r->human = g.human;
    r->pad_ = 0;
    r->boost = st.mono && g.boost ? boost_pitch() : -1;
    r->below = g.boost < 0;
    r->step = art_div(g.rate);
    r->bar = ART_BEAT * 4u;
    r->velocity = velocity;
}
static void build_chord() {
    uint8_t base[ART_CHORD_MAX];
    u32 n = art_base(st.root, (u32)st.kind, st.mask, base);
    int limit = 2 * (int)n;
    int k = clampi(g.cascade, -limit, limit);
    if (g.cascade != st.cascade_was) { st.cascade_was = g.cascade; st.lead_root = -1; }
    if (g.lead && st.nprev && st.lead_root != st.root) {
        st.lead_k = art_lead(st.prev, st.nprev, base, n, g.vtype, limit, st.lead_root < 0 ? k : st.lead_k);
        st.lead_root = st.root;
    }
    if (g.lead && st.lead_root == st.root) k = st.lead_k;
    st.nvoiced = (unsigned char)art_voice(base, n, k, g.vtype, st.voiced);
    if (!st.mono && g.boost) {
        int bp = boost_pitch(), have = 0;
        for (u32 i = 0; i < st.nvoiced; ++i) if (st.voiced[i] == (unsigned char)bp) have = 1;
        if (!have && st.nvoiced < ART_CHORD_MAX) {
            st.voiced[st.nvoiced++] = (unsigned char)bp;
            for (u32 i = 1; i < st.nvoiced; ++i) {
                unsigned char x = st.voiced[i];
                u32 j = i;
                for (; j > 0 && st.voiced[j - 1] > x; --j) st.voiced[j] = st.voiced[j - 1];
                st.voiced[j] = x;
            }
        }
    }
    for (u32 i = 0; i < st.nvoiced; ++i) st.prev[i] = st.voiced[i];
    st.nprev = st.nvoiced;
}
static void schedule(int note, int vel, u32 due, int on) {
    if (st.nsched == MAX_SCHED) return;
    Sched* s = &st.sched[st.nsched++];
    s->note = (unsigned char)note;
    s->vel = (unsigned char)vel;
    s->on = (unsigned char)on;
    s->pad_ = 0;
    s->due = due;
}
static const unsigned char ECHO_TICKS[ECHO_RATES] = { 0, 16, 24, 32, 48, 72, 96, 144, 192 };
static const unsigned char ECHO_FADE[ECHO_REPEATS] = { 181, 128, 91, 64 };
struct Echo { unsigned char n, left, note[ART_CHORD_MAX], vel[ART_CHORD_MAX]; u32 born, next, step; };
static Echo echoes[ECHO_GROUPS];
static volatile u32 echo_taps, echo_skipped;
static void echo_seed(int note, int vel) {
    u32 step = g.echo < ECHO_RATES ? ECHO_TICKS[g.echo] : 0u;
    if (!step) return;
    Echo* e = 0;
    for (int i = 0; i < ECHO_GROUPS && !e; ++i) {
        Echo* c = &echoes[i];
        if (c->left == ECHO_REPEATS && c->born == st.now && c->step == step && c->n < ART_CHORD_MAX) e = c;
    }
    if (!e) {
        int pick = -1;
        u32 oldest = 0;
        for (int i = 0; i < ECHO_GROUPS; ++i) {
            if (!echoes[i].left) { pick = i; break; }
            u32 age = st.now - echoes[i].born;
            if (pick < 0 || age > oldest) { oldest = age; pick = i; }
        }
        e = &echoes[pick];
        e->n = 0;
        e->left = ECHO_REPEATS;
        e->born = st.now;
        e->step = step;
        e->next = st.now + step;
    }
    for (u32 i = 0; i < e->n; ++i) if (e->note[i] == (unsigned char)note) return;
    e->note[e->n] = (unsigned char)note;
    e->vel[e->n] = (unsigned char)vel;
    ++e->n;
}
static int echo_active() {
    for (int i = 0; i < ECHO_GROUPS; ++i) if (echoes[i].left) return 1;
    return 0;
}
static void echo_clear() { for (int i = 0; i < ECHO_GROUPS; ++i) echoes[i].left = 0; }
static void echo_poll() {
    for (int i = 0; i < ECHO_GROUPS; ++i) {
        Echo* e = &echoes[i];
        if (!e->left || (int)(st.now - e->next) < 0) continue;
        u32 fade = ECHO_FADE[ECHO_REPEATS - e->left];
        u32 gate = (e->step * (u32)g.length * 41u) >> 12;
        if (!gate) gate = 1;
        for (u32 k = 0; k < e->n; ++k) {
            int p = e->note[k];
            if (st.count[p] || st.now - st.struck_at[p] < (e->step >> 1) || pitches_sounding() >= SYNTH_VOICES) {
                echo_skipped = echo_skipped + 1;
                continue;
            }
            int v = (int)((e->vel[k] * fade) >> 8);
            if (v < 1) v = 1;
            note_on(p, v);
            schedule(p, v, st.now + gate, 0);
            echo_taps = echo_taps + 1;
        }
        e->next += e->step;
        --e->left;
    }
}
static void strike() {
    art_req r;
    request_of(&r, st.vel * 258);
    art_note nt[ART_LINE_MAX];
    u32 n = art_step(&r, st.pos, &st.rng, nt);
    ++st.pos;
    for (u32 i = 0; i < n; ++i) {
        u32 delay = ticks_of(nt[i].delay), gate = nt[i].gate ? ticks_of(nt[i].gate) : 0u;
        int vel = clampi(nt[i].velocity > 32767 ? 127 : (int)((u32)nt[i].velocity >> 8), 1, 127);
        if (delay == 0) {
            note_on(nt[i].note, vel);
            echo_seed(nt[i].note, vel);
            if (gate) schedule(nt[i].note, vel, st.now + gate, 0);
        } else {
            schedule(nt[i].note, vel, st.now + delay, 1);
            if (gate) schedule(nt[i].note, vel, st.now + delay + gate, 0);
        }
    }
}
static const unsigned char PULSE_DIV[PULSE_RATES] = { 0, 1, 2, 4, 5, 6, 7, 8, 9, 10 };
static u32 pulse_step() { return g.pulse && g.pulse < PULSE_RATES ? ticks_of(art_div(PULSE_DIV[g.pulse])) : 0u; }
static void begin_gesture(int src) {
    let_go_now();
    st.mono = play_mode_mono();
    build_chord();
    art_req r;
    request_of(&r, 1);
    st.on = 1;
    st.src = src;
    st.pos = 0;
    st.anchor = st.now;
    st.period = ticks_of(art_period(&r));
    strike();
    u32 ps = pulse_step();
    st.pulse_next = ps ? st.now + ps : 0;
    st.pulse_note = g.boost ? boost_pitch() : st.root;
}
static void release_gesture() {
    let_go_now();
    st.on = 0;
    st.src = SRC_NONE;
    st.key_id = 0;
    st.latched = 0;
    st.planter = 8;
    st.period = 0;
    st.pulse_next = 0;
}
static u32 swing_of(u32 pos) {
    if (!(pos & 1u) || g.swing <= 50) return 0;
    return (st.period * (u32)(g.swing - 50) * 41u) >> 12;
}
static void drain() {
    for (int i = 0; i < st.nsched;) {
        Sched s = st.sched[i];
        if ((int)(s.due - st.now) > 0) { ++i; continue; }
        st.sched[i] = st.sched[--st.nsched];
        if (s.on) { note_on(s.note, s.vel); echo_seed(s.note, s.vel); }
        else note_off(s.note);
    }
}
static void advance() {
    drain();
    echo_poll();
    if (st.on && st.period) {
        u32 due = st.anchor + st.pos * st.period + swing_of(st.pos);
        if ((int)(st.now - due) >= 0) {
            art_req r;
            request_of(&r, 1);
            if (!r.loop && st.pos >= art_length(&r)) st.period = 0;
            else {
                if (art_strummed(art_style_for(r.style, r.mono))) let_go_now();
                strike();
            }
        }
    }
    if (st.on && st.pulse_next && (int)(st.now - st.pulse_next) >= 0) {
        u32 ps = pulse_step();
        if (ps) {
            note_on(st.pulse_note, st.vel);
            u32 gate = ps >> 1;
            schedule(st.pulse_note, st.vel, st.now + (gate ? gate : 1u), 0);
            st.pulse_next = st.now + ps;
        } else st.pulse_next = 0;
    }
}
static void play_planter(int k, int src) {
    if (!planted(k)) return;
    st.root = clampi(g.pad_root[k], 0, 127);
    g.quality = g.seed_q[k];
    g.extensions = (unsigned short)seed_mask(k);
    g.root = (unsigned char)st.root;
    st.kind = g.quality;
    st.mask = g.extensions;
    if (st.vel <= 0) st.vel = 100;
    begin_gesture(src);
    st.planter = k;
}
static const unsigned char WALK_HALVES[WALK_LENS] = { 1, 2, 4, 8 };
static u32 walk_span() { return (u32)(TICKS_PER_BAR / 2) * WALK_HALVES[g.walk_len < WALK_LENS ? g.walk_len : 1]; }
static int walk_next() {
    int list[8];
    int n = 0, at = 8;
    for (int i = 0; i < 8; ++i) if (planted(i)) { if (i == st.walk_pad) at = n; list[n++] = i; }
    if (!n) return -1;
    int order = g.walk_order;
    if (at == 8) return order == 1 ? list[n - 1] : list[0];
    if (n == 1) return list[0];
    if (order == 1) return list[at ? at - 1 : n - 1];
    if (order == 3) { int k = (int)art_below(&st.rng, (u32)(n - 1)); if (k >= at) ++k; return list[k]; }
    if (order == 2) {
        int d = st.walk_dir ? -1 : 1;
        if (at + d < 0 || at + d >= n) { st.walk_dir ^= 1; d = -d; }
        return list[at + d];
    }
    return list[at + 1 < n ? at + 1 : 0];
}
static void walk_play(int k) {
    play_planter(k, SRC_PAD);
    st.walk_pad = k;
    st.walk_at = st.now;
    st.walk_span = walk_span();
}
static void walk_toggle(int from) {
    if (st.walk) { st.walk = 0; release_gesture(); return; }
    for (int k = 0; k < 8; ++k) {
        int i = (from + k) & 7;
        if (planted(i)) { st.walk = 1; st.walk_dir = 0; walk_play(i); return; }
    }
}
static void walk_poll() {
    if (!st.walk || (u32)(st.now - st.walk_at) < st.walk_span) return;
    int next = walk_next();
    if (next < 0) { st.walk = 0; return; }
    walk_play(next);
}
static void publish_view() {
    u32 n = st.on ? st.nvoiced : 0u;
    for (u32 i = 0; i < n; ++i) view.note[i] = st.voiced[i];
    view.n = (unsigned char)n;
    view.playing = (unsigned char)st.on;
    view.root = (unsigned char)st.root;
    view.kind = (unsigned char)st.kind;
    view.mask = (unsigned short)st.mask;
    view.boost_note = (unsigned char)(g.boost ? boost_pitch() : 0);
    view.boosting = (unsigned char)(st.on && g.boost);
    view.planter = (unsigned char)st.planter;
    view.walking = (unsigned char)st.walk;
    view.stepping = (unsigned char)(st.on && st.period != 0);
    view.echoing = (unsigned char)echo_active();
    view.pos = st.pos;
    view.beat16 = st.on ? (((st.now - st.anchor) / 24u) & 15u) : 16u;
    view.walk_q10 = st.walk && st.walk_span ? udiv((st.now - st.walk_at) << 10, st.walk_span) : 0u;
}
static volatile u32 revoice_req, revoice_done;
static volatile int pad_down_req, pad_up_req, walk_req, hold_req;
static volatile u32 chords, revoices, pads, turns, keys_taken;
static volatile u32 key_calls, release_calls, release_matched, stops, ticks_seen, idle_stops;
static void revoice() {
    if (!st.on) return;
    st.kind = g.quality;
    st.mask = g.extensions;
    begin_gesture(st.src);
}
enum { ASK = 0, KEY_ID_MIDI = 1000, KEY_ID_KEYS = 100 };
static volatile u32 seq_on;
static volatile u32 page_how;
static int synth_live() { return seq_on != 0; }
static void engine_start() {
    if (!st.rng) { st.rng = 0x2545f491u; st.planter = 8; st.lead_root = -1; st.cascade_was = 0x7fffffff; }
}
static int key_id(const Op1fKey* e) { return (e->from == (u32)OP1F_FROM_MIDI ? KEY_ID_MIDI : KEY_ID_KEYS) + e->key + 128; }
static int key_down(const Op1fKey* e) {
    if (e->shift || e->note > 127u) return 0;
    key_calls = key_calls + 1;
    u32 m = mask_off();
    int used = 0;
    if (synth_live() && st.src != SRC_KEY) {
        engine_start();
        st.walk = 0;
        st.key_id = key_id(e);
        st.root = (int)e->note;
        st.vel = clampi((int)e->velocity, 1, 127);
        st.kind = g.quality;
        st.mask = g.extensions;
        st.planter = 8;
        g.root = (unsigned char)e->note;
        begin_gesture(SRC_KEY);
        publish_view();
        used = 1;
        ++chords;
    }
    mask_on(m);
    flush();
    return used;
}
static void key_up(const Op1fKey* e) {
    release_calls = release_calls + 1;
    u32 m = mask_off();
    if (st.src == SRC_KEY && st.key_id == key_id(e)) {
        release_matched = release_matched + 1;
        if (g.hold) { st.latched = 1; st.key_id = 0; st.src = SRC_HELD; }
        else release_gesture();
        publish_view();
    }
    mask_on(m);
    flush();
}
static void engine_stop() {
    u32 m = mask_off();
    release_gesture();
    echo_clear();
    st.walk = 0;
    publish_view();
    mask_on(m);
    flush();
}
extern "C" int garden_key(const Op1fKey* e) OP1F_NAMED("garden_key");
extern "C" int garden_key(const Op1fKey* e) {
    if (e->bytes < 32u || e->host != 0u) return 0;
    if (e->what == (u32)OP1F_UP) { key_up(e); return 1; }
    if (e->what != (u32)OP1F_DOWN) return 0;
    return key_down(e);
}
extern "C" void garden_run(unsigned host, unsigned how) OP1F_NAMED("garden_run");
extern "C" void garden_run(unsigned host, unsigned how) {
    if (host != 0u) return;
    if (how == (u32)OP1F_RUN_ON) { seq_on = 1; engine_start(); return; }
    seq_on = 0;
    stops = stops + 1;
    engine_stop();
}
extern "C" void garden_tick(const Op1fTick* now) OP1F_NAMED("garden_tick");
extern "C" void garden_tick(const Op1fTick* now) {
    if (now->bytes < 32u || now->host != 0u) return;
    ticks_seen = ticks_seen + 1;
    u32 m = mask_off();
    engine_start();
    ++st.now;
    if (!synth_live() && (st.on || st.nsched || echo_active())) {
        release_gesture();
        echo_clear();
        st.walk = 0;
        idle_stops = idle_stops + 1;
    }
    u32 want = revoice_req;
    if (want != revoice_done) { revoice_done = want; revoice(); ++revoices; }
    int down = pad_down_req, up = pad_up_req, walk = walk_req, hold = hold_req;
    pad_down_req = 0;
    pad_up_req = 0;
    walk_req = 0;
    hold_req = 0;
    if (down && synth_live()) {
        int k = down - 1;
        if (st.walk) { st.walk_pad = k; st.walk_at = st.now; }
        if (planted(k)) { play_planter(k, SRC_PAD); ++pads; }
    }
    if (up && st.src == SRC_PAD && !g.hold && !st.walk && st.planter == up - 1) release_gesture();
    if (walk && synth_live()) walk_toggle(walk - 1);
    if (hold && !g.hold && st.latched) release_gesture();
    walk_poll();
    advance();
    publish_view();
    mask_on(m);
    flush();
}
extern "C" void garden_shown(unsigned where, unsigned how) OP1F_NAMED("garden_shown");
extern "C" void garden_shown(unsigned where, unsigned how) { if (where == 0u) page_how = how; }
static void* volatile document;
static volatile u32 page_shown, page_ready;
enum { PAGE_PLAY = 0, PAGE_RHYTHM, PAGE_PALETTE, PAGE_GARDENS, PAGE_HELP };
enum { KEY_HELP = 0, KEY_T1 = 6, KEY_T4 = 9, KEY_CLICK_FIRST = 54, KEY_CLICK_LAST = 57, KEY_PRESS_FIRST = 61, KEY_PRESS_LAST = 64,
       INPUT_L2 = 36, FRAME_L3 = 36, PERIOD_FRAMES = 256 };
static const unsigned char NUMBER_KEY[8] = { 45, 46, 47, 16, 17, 18, 19, 20 };
static volatile int page;
static volatile int help_open;
static volatile u32 help_down_ms;
static volatile int turned_knob = -1;
static volatile u32 turned_ms;
static volatile u32 keys_down;
static int cursor[4];
static int remainder_q10[4];
static struct { u32 since; unsigned char prev, ext, owned, pad_; } modkey[8];
static volatile unsigned char key_did[8];
static int number_of(u32 key) {
    for (int i = 0; i < 8; ++i) if (NUMBER_KEY[i] == key) return i;
    return -1;
}
static void changed(int voice) {
    edits = edits + 1;
    if (voice) revoice_req = revoice_req + 1;
}
static void modifier(int k, int down) {
    u32 now = op1f_ms();
    u32 m = mask_off();
    if (down) {
        modkey[k].since = now;
        modkey[k].owned = 1;
        if (k < 4) { modkey[k].prev = g.quality; g.quality = g.kinds[g.fl][k]; }
        else {
            u32 e = g.extsets[g.pf][k - 4];
            modkey[k].prev = (unsigned char)((g.extensions >> e) & 1u);
            modkey[k].ext = (unsigned char)e;
            g.extensions = (unsigned short)(g.extensions ^ (1u << e));
        }
    } else {
        if (!modkey[k].owned) { mask_on(m); return; }
        modkey[k].owned = 0;
        if (now - modkey[k].since < (u32)MOMENTARY_MS) { mask_on(m); return; }
        if (k < 4) g.quality = modkey[k].prev;
        else {
            u32 e = modkey[k].ext;
            g.extensions = (unsigned short)(modkey[k].prev ? (g.extensions | (1u << e)) : (g.extensions & ~(1u << e)));
        }
    }
    mask_on(m);
    changed(1);
}
static void swap_chords() {
    int slot = -1;
    for (int i = 0; i < 4; ++i) if (g.kinds[g.fl][i] == g.quality) slot = i;
    g.fl ^= 1u;
    if (slot >= 0) g.quality = g.kinds[g.fl][slot];
    for (int i = 0; i < 4; ++i) modkey[i].owned = 0;
}
static void swap_exts() {
    u32 next = 0;
    for (u32 e = 0; e < 12; ++e)
        if ((g.extensions >> e) & 1u) {
            int slot = -1;
            for (int i = 0; i < 4; ++i) if (g.extsets[g.pf][i] == e) slot = i;
            next |= 1u << (slot >= 0 ? g.extsets[g.pf ^ 1u][slot] : e);
        }
    g.pf ^= 1u;
    g.extensions = (unsigned short)next;
    for (int i = 4; i < 8; ++i) modkey[i].owned = 0;
}
static u32 palette_next(int ext, u32 current) {
    u32 used = 0;
    for (int r = 0; r < 2; ++r)
        for (int i = 0; i < 4; ++i) used |= 1u << (ext ? g.extsets[r][i] : g.kinds[r][i]);
    for (u32 k = 1; k <= 12; ++k) {
        u32 v = current + k;
        if (v >= 12) v -= 12;
        if (!((used >> v) & 1u)) return v;
    }
    return current;
}
static void palette_rotate(int q, int slot) {
    int ext = q >= 2, row = q & 1;
    unsigned char* cell = ext ? &g.extsets[row][slot] : &g.kinds[row][slot];
    u32 old = *cell, next = palette_next(ext, old);
    *cell = (unsigned char)next;
    if (!ext && row == g.fl && g.quality == old) g.quality = (unsigned char)next;
    if (ext && row == g.pf && ((g.extensions >> old) & 1u))
        g.extensions = (unsigned short)((g.extensions & ~(1u << old)) | (1u << next));
}
static void palette_reset(int ext) {
    unsigned char (*sets)[4] = ext ? g.extsets : g.kinds;
    int row = ext ? g.pf : g.fl, other = row ^ 1, slot = -1;
    u32 on = 0;
    for (int i = 0; i < 4; ++i) {
        if (!ext && sets[row][i] == g.quality) slot = i;
        if (ext && ((g.extensions >> sets[row][i]) & 1u)) { on |= 1u << i; g.extensions &= (unsigned short)~(1u << sets[row][i]); }
    }
    for (int i = 0; i < 4; ++i) sets[row][i] = (unsigned char)(4 * row + i);
    for (int i = 0; i < 4; ++i) {
        int clash = 0;
        for (int j = 0; j < 4; ++j) if (sets[other][i] == sets[row][j]) clash = 1;
        if (!clash) continue;
        u32 used = 0;
        for (int r = 0; r < 2; ++r) for (int k = 0; k < 4; ++k) used |= 1u << sets[r][k];
        for (u32 x = 0; x < 12; ++x) if (!((used >> x) & 1u)) { sets[other][i] = (unsigned char)x; break; }
    }
    if (!ext && slot >= 0) g.quality = sets[row][slot];
    for (int i = 0; i < 4; ++i) if ((on >> i) & 1u) g.extensions |= (unsigned short)(1u << sets[row][i]);
}
static u32 choice_step(int ext, u32 current, int d) {
    unsigned char list[13];
    u32 n = 0, seen = 0;
    list[n++] = NONE;
    for (int r = 0; r < 2; ++r)
        for (int i = 0; i < 4; ++i) {
            u32 v = ext ? g.extsets[r][i] : g.kinds[r][i];
            if (!((seen >> v) & 1u)) { seen |= 1u << v; list[n++] = (unsigned char)v; }
        }
    for (u32 v = 0; v < 12; ++v) if (!((seen >> v) & 1u)) list[n++] = (unsigned char)v;
    int at = 0;
    for (u32 i = 0; i < n; ++i) if (list[i] == current) at = (int)i;
    int k = at + d;
    while (k < 0) k += (int)n;
    while (k >= (int)n) k -= (int)n;
    return list[k];
}
static void plant_now(int i) {
    g.seed_q[i] = g.quality;
    int k = 0;
    for (int j = 0; j < 4; ++j) g.seed_ext[i][j] = NONE;
    for (u32 e = 0; e < 12 && k < 4; ++e) if ((g.extensions >> e) & 1u) g.seed_ext[i][k++] = (unsigned char)e;
    g.pad_root[i] = g.root;
    g.pad_used[i] = 1;
}
static void turn_play(int knob, int steps, int shift) {
    int style = g.style, arp = style == STYLE_ARP || style == STYLE_FARP;
    if (knob == 0) {
        if (shift) { g.echo = (unsigned char)clampi(g.echo + steps, 0, ECHO_RATES - 1); return; }
        int limit = 2 * (int)tones_of(g.quality, g.extensions);
        g.cascade = (signed char)clampi(g.cascade + steps, -limit, limit);
    } else if (knob == 1) {
        if (shift) g.rate = (unsigned char)clampi(g.rate + steps, 0, 11);
        else {
            int v = clampi(style + steps, 0, 7), dir = steps > 0 ? 1 : -1;
            if (play_mode_mono())
                while (v == STYLE_CHORD || v == STYLE_HARP) { v += dir; if (v < 0 || v > 7) { v = style; break; } }
            g.style = (unsigned char)v;
        }
    } else if (knob == 2) {
        int phrase = style == STYLE_PATTERN && !shift, order = style != STYLE_PATTERN && (arp ^ shift);
        if (phrase) g.phrase = (unsigned char)clampi(g.phrase + steps, 0, (int)ART_PHRASES - 1);
        else if (order) g.order = (unsigned char)clampi(g.order + steps, 0, 5);
        else g.vtype = (unsigned char)clampi(g.vtype + steps, 0, 5);
    } else {
        if (shift) g.pulse = (unsigned char)clampi(g.pulse + steps, 0, PULSE_RATES - 1);
        else g.boost = (signed char)clampi(g.boost + steps, -2, 2);
    }
}
static void turn_rhythm(int knob, int steps) {
    if (knob == 0) g.loop = (unsigned char)clampi(g.loop + steps, 0, 1);
    else if (knob == 1) g.length = (unsigned char)clampi(g.length + 5 * steps, 1, 100);
    else if (knob == 2) g.swing = (unsigned char)clampi(g.swing + steps, 50, 75);
    else g.human = (unsigned char)clampi(g.human + 5 * steps, 0, 100);
}
static void turn_gardens(int knob, int steps, int shift) {
    int pad = g.pad_sel;
    if (shift) {
        if (knob == 0) g.walk_len = (unsigned char)clampi(g.walk_len + steps, 0, WALK_LENS - 1);
        else if (knob == 1) g.walk_order = (unsigned char)clampi(g.walk_order + steps, 0, WALK_ORDERS - 1);
        return;
    }
    if (knob == 0) g.pad_sel = (unsigned char)clampi(pad + steps, 0, 7);
    else if (knob == 1) {
        g.pad_root[pad] = (unsigned char)clampi(g.pad_root[pad] + steps, 0, 127);
        if (g.seed_q[pad] == NONE) g.seed_q[pad] = g.quality;
        g.pad_used[pad] = g.seed_q[pad] < 12;
    } else if (knob == 2) {
        g.seed_q[pad] = (unsigned char)choice_step(0, g.seed_q[pad], steps);
        if (!g.pad_used[pad]) g.pad_root[pad] = g.root;
        g.pad_used[pad] = g.seed_q[pad] < 12;
    } else {
        int sq = g.square & 3;
        g.seed_ext[pad][sq] = (unsigned char)choice_step(1, g.seed_ext[pad][sq], steps);
    }
}
static void turn(int knob, int q10, int shift) {
    int sum = clampi(q10, -1048576, 1048576) + remainder_q10[knob], steps = sum / 1024;
    remainder_q10[knob] = sum - steps * 1024;
    if (!steps) return;
    turns = turns + 1;
    int pg = page;
    if (pg == PAGE_PLAY) { turned_knob = knob; turned_ms = op1f_ms(); }
    u32 m = mask_off();
    if (pg == PAGE_PLAY) turn_play(knob, steps, shift);
    else if (pg == PAGE_RHYTHM) turn_rhythm(knob, steps);
    else if (pg == PAGE_PALETTE) cursor[knob] = clampi(cursor[knob] + steps, 0, 3);
    else turn_gardens(knob, steps, shift);
    mask_on(m);
    changed(pg == PAGE_PLAY || pg == PAGE_RHYTHM);
}
static void push(int knob, int shift) {
    int pg = page, voice = 0, edit = 1;
    u32 m = mask_off();
    if (pg == PAGE_PLAY) {
        if (shift) edit = 0;
        else if (knob == 0) { g.lead ^= 1u; voice = 1; }
        else if (knob == 1) { swap_chords(); voice = 1; }
        else if (knob == 2) { g.hold ^= 1u; if (!g.hold) hold_req = 1; }
        else { swap_exts(); voice = 1; }
    } else if (pg == PAGE_PALETTE) {
        if (shift) {
            if (knob == 0) swap_chords();
            else if (knob == 1) palette_reset(0);
            else if (knob == 2) swap_exts();
            else palette_reset(1);
        } else palette_rotate(knob, cursor[knob] & 3);
        voice = 1;
    } else if (pg == PAGE_GARDENS) {
        if (shift) { if (knob == 0) walk_req = g.pad_sel + 1; edit = 0; }
        else if (knob == 0) plant_now(g.pad_sel);
        else if (knob == 3) g.square = (unsigned char)((g.square + 1) & 3);
        else edit = 0;
    } else edit = 0;
    mask_on(m);
    if (edit) changed(voice);
}
extern "C" int garden_input(const Op1fInput* e) OP1F_NAMED("garden_input");
extern "C" int garden_input(const Op1fInput* e) {
    if (e->bytes < (u32)INPUT_L2) return 0;
    u32 key = e->key;
    if (e->what == (u32)OP1F_UP) {
        if (key == (u32)KEY_HELP) {
            if (help_open && op1f_ms() - help_down_ms >= (u32)MOMENTARY_MS) help_open = 0;
            return 1;
        }
        int k = number_of(key);
        if (k >= 0) {
            keys_down = keys_down & ~(1u << (u32)k);
            if (key_did[k] == 1) modifier(k, 0);
            else if (key_did[k] == 2) pad_up_req = k + 1;
            key_did[k] = 0;
        }
        return 1;
    }
    if (!e->mine) return 0;
    if (e->what == (u32)OP1F_TURN) {
        if (e->key < 4u && !help_open) turn((int)e->key, e->turn, e->shift != 0);
        return 1;
    }
    if (e->what != (u32)OP1F_DOWN && e->what != (u32)OP1F_DOWN_SHIFT) return 0;
    int shifted = e->what == (u32)OP1F_DOWN_SHIFT;
    if (key == (u32)KEY_HELP) {
        if (shifted) return 0;
        if (help_open) help_open = 0;
        else { help_down_ms = op1f_ms(); help_open = 1; }
        return 1;
    }
    if (key >= (u32)KEY_CLICK_FIRST && key <= (u32)KEY_CLICK_LAST) {
        if (!help_open) push((int)(key - KEY_CLICK_FIRST), shifted);
        return 1;
    }
    if (key >= (u32)KEY_PRESS_FIRST && key <= (u32)KEY_PRESS_LAST) return 1;
    if (key >= (u32)KEY_T1 && key <= (u32)KEY_T4) {
        help_open = 0;
        page = (int)(key - KEY_T1);
        for (int i = 0; i < 4; ++i) remainder_q10[i] = 0;
        return 1;
    }
    int k = number_of(key);
    if (k >= 0) {
        keys_taken = keys_taken + 1;
        if (page == PAGE_GARDENS) {
            if (shifted) {
                u32 m = mask_off();
                plant_now(k);
                g.pad_sel = (unsigned char)k;
                mask_on(m);
                key_did[k] = 3;
                changed(0);
                return 1;
            }
            keys_down = keys_down | (1u << (u32)k);
            key_did[k] = 2;
            pad_down_req = k + 1;
            return 1;
        }
        if (shifted) return 0;
        keys_down = keys_down | (1u << (u32)k);
        key_did[k] = 1;
        modifier(k, 1);
        return 1;
    }
    return 0;
}
enum { Q = 1024 };
static void* part[PART_COUNT];
static unsigned char want[PART_COUNT], have[PART_COUNT];
static u32 parts_found;
static u32 thin_used[sizeof THIN_N], thick_used[sizeof THICK_N];
static unsigned char thin_dropped, thick_dropped;
static void use(int i) { want[i] = 1; }
static void pose(int i, int a, int b, int c, int d, int x, int y) {
    want[i] = 1;
    if (part[i]) {
        int m[9] = { a, b, 0, c, d, 0, x, y, Q };
        op1f_move(part[i], m);
    }
}
static void place(int i, int x, int y, int scale) { pose(i, scale, 0, 0, scale, x * Q, y * Q); }
static void segment(int i, int x1, int y1, int x2, int y2) {
    int dx = x2 - x1, dy = y2 - y1;
    int sx = dx >> 6, sy = dy >> 6;
    int len = (int)isqrt((u32)(sx * sx + sy * sy));
    if (len == 0) return;
    pose(i, dx, dy, -sdiv(dy * 16, len), sdiv(dx * 16, len), x1, y1);
}
static void thin(int pool, int x1, int y1, int x2, int y2) {
    if (thin_used[pool] >= THIN_N[pool]) { thin_dropped = 1; return; }
    segment(THIN_AT[pool] + (int)thin_used[pool]++, x1, y1, x2, y2);
}
static void thick(int pool, int x1, int y1, int x2, int y2) {
    if (thick_used[pool] >= THICK_N[pool]) { thick_dropped = 1; return; }
    segment(THICK_AT[pool] + (int)thick_used[pool]++, x1, y1, x2, y2);
}
static void commit() {
    for (int i = 0; i < PART_COUNT; ++i) {
        if (part[i]) op1f_show(part[i], want[i]);
        have[i] = want[i];
        want[i] = 0;
    }
    for (u32 i = 0; i < sizeof THIN_N; ++i) thin_used[i] = 0;
    for (u32 i = 0; i < sizeof THICK_N; ++i) thick_used[i] = 0;
}
static void* font[TEXT_COUNT];
static char font_text[TEXT_COUNT][24];
static char font_name[TEXT_COUNT][5];
static unsigned char font_made[TEXT_COUNT];
static volatile u32 texts_set, texts_refused;
static void make_fonts() {
    for (int t = 0; t < TEXT_COUNT; ++t) {
        int v = t, at = 1;
        font_name[t][0] = 't';
        if (v >= 10) { int tens = 0; while (v >= 10) { v -= 10; ++tens; } font_name[t][at++] = (char)('0' + tens); }
        font_name[t][at++] = (char)('0' + v);
        font_name[t][at] = 0;
        font_text[t][0] = 0;
        font_made[t] = 1;
    }
}
static void say(int t, int x, int y, const char* s) {
    if (t < 0 || t >= TEXT_COUNT || !font_made[t] || !document) return;
    char small[24];
    int n = 0;
    for (; n < 23 && s[n]; ++n) {
        unsigned char c = (unsigned char)s[n];
        if (c < 0x20 || c > 0x7e) c = '?';
        small[n] = c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : (char)c;
    }
    small[n] = 0;
    int same = font[t] != 0;
    for (int i = 0; same && i <= n; ++i) if (font_text[t][i] != small[i]) same = 0;
    if (!same) {
        if (!op1f_text_set(document, font_name[t], small, 480, TEXT_ALIGN[t])) {
            texts_refused = texts_refused + 1;
            font_made[t] = 0;
            return;
        }
        for (int i = 0; i <= n; ++i) font_text[t][i] = small[i];
        if (!font[t]) font[t] = op1f_text_node(document, font_name[t]);
        texts_set = texts_set + 1;
    }
    if (n && font[t]) {
        int m[9] = { Q, 0, 0, 0, Q, 0, x * Q, y * Q, Q };
        op1f_move(font[t], m);
    }
}
static void hush(int t) { say(t, 0, 0, ""); }
struct Line {
    char s[40];
    int n;
    void clear() { n = 0; s[0] = 0; }
    void put(const char* t) { while (*t && n < (int)sizeof s - 1) s[n++] = *t++; s[n] = 0; }
    void num(int v) {
        static const u32 tens[3] = { 100u, 10u, 1u };
        u32 u = v < 0 ? (u32)-v : (u32)v;
        if (v < 0) put("-");
        if (u > 999u) u = 999u;
        int started = 0;
        for (int i = 0; i < 3; ++i) {
            char c[2] = { '0', 0 };
            while (u >= tens[i]) { u -= tens[i]; ++c[0]; }
            if (c[0] != '0' || started || i == 2) { put(c); started = 1; }
        }
    }
};
static const char* const NOTE_NAME[12] = { "c", "c#", "d", "d#", "e", "f", "f#", "g", "g#", "a", "a#", "b" };
static const char* const STYLE_NAME[8] = { "chord", "strum", "strum 2", "slop", "arp", "farp", "pattern", "harp" };
static const char RATE_NAME[12][6] = { "2/1", "1/1", "1/2", "1/2t", "1/4", "1/4t", "1/8", "1/8t", "1/16", "1/16t", "1/32", "1/32t" };
static const char* const ORDER_NAME[6] = { "up", "down", "updown", "downup", "inside", "random" };
static const char* const VTYPE_NAME[6] = { "close", "drop2", "drop3", "open", "shell", "spread" };
static const char* const ECHO_NAME[ECHO_RATES] = { "no echo", "e 1/16t", "e 1/16", "e 1/8t", "e 1/8", "e 1/8d", "e 1/4", "e 1/4d", "e 1/2" };
static const char* const PULSE_NAME[PULSE_RATES] = { "no pulse", "p 1/1", "p 1/2", "p 1/4", "p 1/4t", "p 1/8", "p 1/8t", "p 1/16", "p 1/16t", "p 1/32" };
static const char* const BOOST_NAME[5] = { "boost-2", "boost-1", "boost 0", "boost+1", "boost+2" };
static const char* const WALK_LEN_NAME[WALK_LENS] = { "half bar", "1 bar", "2 bars", "4 bars" };
static const char* const WALK_ORDER_NAME[WALK_ORDERS] = { "1 to 8", "8 to 1", "bounce", "random" };
static const short SIN[256] = {
    0, 402, 804, 1205, 1606, 2006, 2404, 2801, 3196, 3590, 3981, 4370, 4756, 5139, 5520, 5897, 6270, 6639, 7005, 7366,
    7723, 8076, 8423, 8765, 9102, 9434, 9760, 10080, 10394, 10702, 11003, 11297, 11585, 11866, 12140, 12406, 12665,
    12916, 13160, 13395, 13623, 13842, 14053, 14256, 14449, 14635, 14811, 14978, 15137, 15286, 15426, 15557, 15679,
    15791, 15893, 15986, 16069, 16143, 16207, 16261, 16305, 16340, 16364, 16379, 16384, 16379, 16364, 16340, 16305,
    16261, 16207, 16143, 16069, 15986, 15893, 15791, 15679, 15557, 15426, 15286, 15137, 14978, 14811, 14635, 14449,
    14256, 14053, 13842, 13623, 13395, 13160, 12916, 12665, 12406, 12140, 11866, 11585, 11297, 11003, 10702, 10394,
    10080, 9760, 9434, 9102, 8765, 8423, 8076, 7723, 7366, 7005, 6639, 6270, 5897, 5520, 5139, 4756, 4370, 3981, 3590,
    3196, 2801, 2404, 2006, 1606, 1205, 804, 402, 0, -402, -804, -1205, -1606, -2006, -2404, -2801, -3196, -3590, -3981,
    -4370, -4756, -5139, -5520, -5897, -6270, -6639, -7005, -7366, -7723, -8076, -8423, -8765, -9102, -9434, -9760,
    -10080, -10394, -10702, -11003, -11297, -11585, -11866, -12140, -12406, -12665, -12916, -13160, -13395, -13623,
    -13842, -14053, -14256, -14449, -14635, -14811, -14978, -15137, -15286, -15426, -15557, -15679, -15791, -15893,
    -15986, -16069, -16143, -16207, -16261, -16305, -16340, -16364, -16379, -16384, -16379, -16364, -16340, -16305,
    -16261, -16207, -16143, -16069, -15986, -15893, -15791, -15679, -15557, -15426, -15286, -15137, -14978, -14811,
    -14635, -14449, -14256, -14053, -13842, -13623, -13395, -13160, -12916, -12665, -12406, -12140, -11866, -11585,
    -11297, -11003, -10702, -10394, -10080, -9760, -9434, -9102, -8765, -8423, -8076, -7723, -7366, -7005, -6639, -6270,
    -5897, -5520, -5139, -4756, -4370, -3981, -3590, -3196, -2801, -2404, -2006, -1606, -1205, -804, -402 };
static inline __attribute__((always_inline)) int wobble(u32 ms, u32 period, u32 per_q16, int phase, int amp) {
    u32 angle = ((ms % period) * per_q16) >> 16;
    return (amp * SIN[(int)(angle + (u32)phase) & 255]) >> 14;
}
struct Voiced { unsigned char n, note[9], role[9]; };
static int role_of(int root, int quality, u32 mask, int note) {
    int pc = mod12(note - root);
    if (!pc) return 0;
    for (u32 i = 0; i < art_kind_count[quality]; ++i) if (mod12(art_kind_tones[quality][i]) == pc) return 1;
    for (u32 e = 0; e < 12; ++e) if (((mask >> e) & 1u) && mod12(art_pollen_interval[e]) == pc) return 2 + (int)e;
    return 1;
}
static int sounding(int n) { return n >= 0 && n < 128 && st.count[n] != 0; }
static int any_sounding() { for (int n = 0; n < 128; ++n) if (st.count[n]) return 1; return 0; }
static void voice_of(Voiced* out) {
    out->n = 0;
    int root = g.root, q = g.quality;
    u32 mask = g.extensions;
    unsigned char notes[ART_CHORD_MAX];
    u32 k = 0;
    if (view.playing && view.n) {
        root = view.root;
        q = view.kind < 12 ? view.kind : q;
        mask = view.mask;
        k = view.n;
        for (u32 i = 0; i < k; ++i) notes[i] = view.note[i];
    } else {
        uint8_t base[ART_CHORD_MAX];
        u32 n = art_base(root, (u32)q, mask, base);
        k = art_voice(base, n, g.cascade, g.vtype, notes);
    }
    int boost = g.boost ? fold(root + 12 * g.boost) : -1;
    for (u32 i = 0; i < k && out->n < 9; ++i) {
        out->note[out->n] = notes[i];
        out->role[out->n] = (unsigned char)(notes[i] == boost && boost != root ? 15 : role_of(root, q, mask, notes[i]));
        ++out->n;
    }
    if (boost >= 0 && !(view.playing && view.n) && out->n < 9) {
        u32 i = out->n++;
        out->note[i] = (unsigned char)boost;
        out->role[i] = 15;
        for (; i > 0 && out->note[i - 1] > out->note[i]; --i) {
            unsigned char a = out->note[i]; out->note[i] = out->note[i - 1]; out->note[i - 1] = a;
            a = out->role[i]; out->role[i] = out->role[i - 1]; out->role[i - 1] = a;
        }
    }
}
static int thin_role(int role, int quality, int lit) {
    if (lit) return THIN_WHITE;
    if (role == 0) return THIN_OCHRE;
    if (role == 15) return THIN_ORANGE;
    return THIN_HUE0 + (role == 1 ? quality : (role - 2));
}
static int thick_role(int role, int quality) {
    if (role == 0) return THICK_OCHRE;
    if (role == 15) return THICK_ORANGE;
    return THICK_HUE0 + (role == 1 ? quality : (role - 2));
}
static u32 icon_used[12], icon_on_used;
static void icon_on(int x, int y, int scale) {
    if (icon_on_used < 9u) place(ICONON_AT + (int)icon_on_used++, x, y, scale);
}
static void icon(int kind, int x, int y, int scale) {
    if (kind < 0 || kind > 11 || icon_used[kind] >= (u32)ICON_COPIES) return;
    place(ICON_AT + kind * ICON_COPIES + (int)icon_used[kind]++, x, y, scale);
}
static void beat_dots() {
    u32 b = view.beat16;
    for (int k = 0; k < 4; ++k) {
        int on = b < 16u && (int)(b & 3u) == k;
        use(DOT_AT + k * 3 + (on ? (b < 4u ? 2 : 1) : 0));
    }
}
static void status_row(u32 ms, int shift) {
    int style = g.style, arp = style == STYLE_ARP || style == STYLE_FARP;
    Line l[4];
    for (int i = 0; i < 4; ++i) l[i].clear();
    if (shift) l[0].put(ECHO_NAME[g.echo < ECHO_RATES ? g.echo : 0]);
    else { if (g.cascade > 0) l[0].put("+"); l[0].num(g.cascade); }
    l[1].put(shift ? RATE_NAME[g.rate] : STYLE_NAME[style]);
    if (style == STYLE_PATTERN) l[2].put(shift ? VTYPE_NAME[g.vtype] : art_phrase_names[g.phrase]);
    else if (arp ^ shift) l[2].put(ORDER_NAME[g.order]);
    else l[2].put(VTYPE_NAME[g.vtype]);
    l[3].put(shift ? PULSE_NAME[g.pulse] : BOOST_NAME[g.boost + 2]);
    for (int i = 0; i < 4; ++i) say(TX_ST0 + i, 60 + 120 * i, 26, l[i].s);
    int fk = turned_knob;
    if (fk >= 0 && fk < 4 && ms - turned_ms < 1000u) {
        int c = 60 + 120 * fk, half = (13 * l[fk].n) / 2;
        thin(THIN_KNOB0 + fk, (c - half) * Q, 35 * Q, (c + half) * Q, 35 * Q);
    }
    if (!shift) {
        const unsigned char chips[4] = { g.lead, g.hold, g.fl, g.pf };
        for (int k = 0; k < 4; ++k) {
            int dx = -(13 * l[k].n) / 2;
            if (k == 0) place(STAIR, dx, 0, Q);
            for (int b = 0; b < 2; ++b) place(CHIP_AT + (k * 2 + b) * 2 + (b == chips[k] ? 1 : 0), dx, 0, Q);
        }
    }
}
static const short HOVER[4][2] = { { 150, 58 }, { 162, 100 }, { 330, 58 }, { 318, 100 } };
static u32 hit_ms[128];
static unsigned char was_on[16];
static unsigned char slide_note[9], slide_n;
static short slide_x[9], slide_from[9];
static u32 slide_t0;
static void track_hits(u32 ms) {
    for (int n = 0; n < 128; ++n) {
        int on = sounding(n), was = (was_on[n >> 3] >> (n & 7)) & 1;
        if (on && !was) hit_ms[n] = ms ? ms : 1u;
        if (on) was_on[n >> 3] = (unsigned char)(was_on[n >> 3] | (1u << (n & 7)));
        else was_on[n >> 3] = (unsigned char)(was_on[n >> 3] & ~(1u << (n & 7)));
    }
}
static int flash_of(u32 ms, int n) {
    if (n < 0 || n > 127 || !hit_ms[n] || !sounding(n)) return 0;
    u32 age = ms - hit_ms[n];
    return age >= 240u ? 0 : (int)(256u - ((age * 1093u) >> 10));
}
static int voicing_mul(int vtype, int j, int n) {
    switch (vtype) {
    case 1: return j == (n >= 2 ? n - 2 : 2 * n - 2) ? 132 : 100;
    case 2: return j == (n >= 3 ? n - 3 : 2 * n - 3) ? 132 : 100;
    case 3: return (j & 1) ? 120 : 85;
    case 4: return (j & 1) ? 55 : 100;
    case 5: return 118;
    default: return 100;
    }
}
static void roots(u32 ms, const Voiced* vc) {
    if (!vc->n) { slide_n = 0; return; }
    int lo = vc->note[0], hi = vc->note[vc->n - 1], span = hi - lo < 24 ? 24 : hi - lo, xs[9], prev = -1000;
    for (u32 i = 0; i < vc->n; ++i) {
        int x = 28 + sdiv((vc->note[i] - lo) * 400, span);
        if (x < prev + 28) x = prev + 28;
        xs[i] = x;
        prev = x;
    }
    int over = xs[vc->n - 1] - 452;
    if (over > 0) for (u32 i = 0; i < vc->n; ++i) xs[i] -= over;
    int changed = vc->n != slide_n;
    for (u32 i = 0; i < vc->n && !changed; ++i) if (vc->note[i] != slide_note[i]) changed = 1;
    if (changed) {
        u32 age0 = ms - slide_t0;
        int k0 = age0 >= 260u ? 256 : (int)((age0 * 1008u) >> 10);
        short shown[9];
        for (u32 j = 0; j < slide_n; ++j) shown[j] = (short)(slide_from[j] + (((slide_x[j] - slide_from[j]) * k0) >> 8));
        for (u32 i = 0; i < vc->n; ++i) {
            int from = xs[i], found = 0;
            for (u32 j = 0; j < slide_n && !found; ++j) if (slide_note[j] == vc->note[i]) { from = shown[j]; found = 1; }
            for (u32 j = 0; j < slide_n && !found; ++j) if (mod12(slide_note[j] - vc->note[i]) == 0) { from = shown[j]; found = 1; }
            slide_from[i] = (short)from;
        }
        for (u32 i = 0; i < vc->n; ++i) { slide_note[i] = vc->note[i]; slide_x[i] = (short)xs[i]; }
        slide_n = vc->n;
        slide_t0 = ms;
    }
    u32 age = ms - slide_t0;
    int e = age >= 260u ? 256 : (int)((age * 1008u) >> 10);
    int k = 256 - (((((256 - e) * (256 - e)) >> 8) * (256 - e)) >> 8);
    for (u32 i = 0; i < vc->n; ++i) {
        int r = vc->role[i], ax = r == 15 ? 62 : 240, px = slide_from[i] + (((xs[i] - slide_from[i]) * k) >> 8);
        int pool = thin_role(r, g.quality, sounding(vc->note[i]));
        int x0 = ax * Q, y0 = 123 * Q;
        for (int s = 1; s <= 4; ++s) {
            int u = 4 - s, t = s;
            int bx = (u * u * u * ax + 3 * u * u * t * ax + 3 * u * t * t * px + t * t * t * px) * (Q / 64);
            int by = (u * u * u * 123 + 3 * u * u * t * 132 + 3 * u * t * t * 128 + t * t * t * 138) * (Q / 64);
            thin(pool, x0, y0, bx, by);
            x0 = bx;
            y0 = by;
        }
        if (r >= 2 && r < 15) {
            static const unsigned char H3[6][2] = { { 0, 0 }, { 1, 0 }, { 2, 0 }, { 0, 1 }, { 1, 1 }, { 2, 1 } };
            for (int h = 0; h < 6; ++h) {
                int s4 = 2 + H3[h][0], u = 4 - s4, t = s4;
                int hx = (u * u * u * ax + 3 * u * u * t * ax + 3 * u * t * t * px + t * t * t * px) >> 6;
                int hy = (u * u * u * 123 + 3 * u * u * t * 132 + 3 * u * t * t * 128 + t * t * t * 138) >> 6;
                int side = (h & 1) ? 1 : -1, len = 3 + (h & 1);
                hx += H3[h][1] * (px - ax > 0 ? -3 : 3);
                thin(pool, hx * Q, hy * Q, (hx + side * len) * Q, (hy + len) * Q);
            }
        }
        say(TX_NOTE0 + (int)i, px, 158, NOTE_NAME[mod12(vc->note[i])]);
    }
}
static void ripples(u32 ms, int moving) {
    if (!g.echo || g.echo >= ECHO_RATES) return;
    int spacing = 7 + 2 * g.echo, ph = moving ? (int)udiv(umod(ms, 833u) * (u32)spacing, 833u) : 0;
    for (int k = 0; k < ECHO_REPEATS; ++k) {
        int w = 16 + spacing * k + ph;
        thin(THIN_ORANGE, (240 - w + 6) * Q, 120 * Q, (240 - w) * Q, 117 * Q);
        thin(THIN_ORANGE, (240 - w) * Q, 117 * Q, (240 - w - 4) * Q, 120 * Q);
        thin(THIN_ORANGE, (240 + w - 6) * Q, 120 * Q, (240 + w) * Q, 117 * Q);
        thin(THIN_ORANGE, (240 + w) * Q, 117 * Q, (240 + w + 4) * Q, 120 * Q);
    }
}
static void play_page(u32 ms, int shift) {
    use(PAGE1);
    status_row(ms, shift);
    beat_dots();
    track_hits(ms);
    Voiced vc;
    voice_of(&vc);
    int moving = any_sounding();
    int fv = clampi(g.cascade + 8, 0, 16), sy = 108 - fv * 62 / 16;
    place(SUN, 446, sy, Q);
    place(RAYS_AT + fv, 446, sy, Q);
    if (fv > 8 || g.lead) place(SUNFILL, 446, sy, Q);
    if (g.lead) place(LEAD_AT + fv, 446, sy, Q);
    if (fv < 7) use(HORIZON);
    int per = GROUND_PERIOD[g.style & 7];
    if (per) {
        u32 t = umod(ms, 100u * (u32)per);
        int slide = moving ? (int)umod(udiv(t * 3u, 100u), (u32)per) : 0;
        place(GROUND_AT + g.style, -slide, 0, Q);
        use(GMASK);
    } else use(GROUND_AT + g.style);
    if (g.style == STYLE_HARP)
        for (int i = 0; i < HARP_N; ++i) {
            int lean = moving ? wobble(ms, 500u, 33554u, HARP_X[i], 2) : 0;
            pose(HARP_AT + i, Q, 0, -lean * 171, Q, HARP_X[i] * Q, 122 * Q);
        }
    int bob = moving ? (g.pulse ? wobble(ms, 260u, 64527u, 0, 3) : wobble(ms, 750u, 22370u, 0, 2)) : 0;
    place(MOLE_AT + clampi(g.boost + 2, 0, 4), 62, 122 + bob, Q);
    ripples(ms, moving);
    roots(ms, &vc);
    for (u32 i = vc.n; i < 9; ++i) hush(TX_NOTE0 + (int)i);
    for (int i = 0; i < 8; ++i) {
        int x = 30 + 60 * i, held = (int)((keys_down >> (u32)i) & 1u), blink = held && ((ms / 150u) & 1u);
        if (i < 4) {
            int k = g.kinds[g.fl][i];
            int on = k == g.quality;
            if (on) use((blink ? KRINGW_AT : KRING_AT) + i);
            else icon(k, x, 179, Q);
            say(TX_KLW0 + i, x, 210, on ? art_kind_names[k] : "");
            say(TX_KLG0 + i, x, 210, on ? "" : art_kind_names[k]);
        } else {
            int e = g.extsets[g.pf][i - 4];
            int on = (g.extensions >> e) & 1u;
            if (on) use((blink ? KRINGW_AT : KRING_AT) + i);
            if (art_pollen_silent(g.quality, (u32)e)) use(MUTE_AT + i - 4);
            say(TX_KLW0 + i, x, 210, on ? art_pollen_names[e] : "");
            say(TX_KLG0 + i, x, 210, on ? "" : art_pollen_names[e]);
        }
        if (held) use((blink ? HELDW_AT : HELD_AT) + i);
    }
    int rank = 0;
    for (u32 e = 0; e < 12; ++e) {
        if ((g.extensions >> e) & 1u) {
            if (rank >= 4) continue;
            int w = moving ? wobble(ms, 1200u, 13981u, rank * 45, 3) : wobble(ms, 3000u, 5592u, rank * 45, 2);
            place(CRITTER_AT + (int)e, HOVER[rank][0], HOVER[rank][1] + w, 1178);
            ++rank;
        } else {
            for (int i = 0; i < 4; ++i) if (g.extsets[g.pf][i] == e) place(CRITTER_AT + (int)e, 30 + 60 * (4 + i), 179, Q);
        }
    }
    int kind = g.quality, n = PETALS[kind];
    const char* name = NOTE_NAME[mod12(view.playing ? view.root : g.root)];
    int wide = name[1] != 0, sway = moving ? wobble(ms, 3700u, 4534u, 0, 2) : 0;
    place(wide ? DISC2 : DISC1, sway, 0, Q);
    unsigned char tone[9];
    u32 tones = 0;
    for (u32 i = 0; i < vc.n; ++i) if (vc.role[i] != 15) tone[tones++] = vc.note[i];
    for (int j = 0; j < n; ++j) {
        int lit = 0, fl = 0;
        for (u32 i = 0; i < tones; ++i) {
            int a0 = (int)udiv(i * (u32)n, tones), a1 = (int)udiv((i + 1) * (u32)n, tones);
            if (a1 == a0) a1 = a0 + 1;
            if (!((j >= a0 && j < a1) || (tones > (u32)n && (int)umod(i, (u32)n) == j))) continue;
            if (sounding(tone[i])) lit = 1;
            int f = flash_of(ms, tone[i]);
            if (f > fl) fl = f;
        }
        int len = 28 * voicing_mul(g.vtype, j, n) + (moving ? 100 * wobble(ms, 2100u, 7990u, j * 53, 2) : 0) + 600 * fl / 256;
        int sq = sdiv(len * (Q / 4), 28 * 25);
        int turn = moving ? wobble(ms, 7000u, 2397u, j * 40, 3) : 0;
        int ca = (sq * SIN[(turn + 64) & 255]) >> 14, sa = (sq * SIN[turn & 255]) >> 14;
        int state = fl > 77 ? 2 : lit ? 1 : 0;
        pose(PETAL_AT + PETAL_OF[kind] + (wide * n + j) * 3 + state, ca, sa, -sa, ca,
             (240 + sway) * Q - (ca * 240 - sa * 78), 78 * Q - (sa * 240 + ca * 78));
    }
    say(TX_ROOT, 240 + sway, 85, name);
}
static void rhythm_page() {
    use(PAGE2);
    beat_dots();
    static const unsigned char UNITS[12] = { 192, 96, 48, 32, 24, 16, 12, 8, 6, 4, 3, 2 };
    const int X0 = 104, X1 = 376, Y0 = 22, Y1 = 166, W = X1 - X0;
    Voiced vc;
    voice_of(&vc);
    int n = vc.n ? vc.n : 1, style = g.style;
    int step = UNITS[g.rate < 12 ? g.rate : 6], rows = n + 1 < 4 ? 4 : n + 1, rowh = sdiv(Y1 - Y0, rows), drawn = 0;
    int arp = style == STYLE_ARP || style == STYLE_FARP || style == STYLE_PATTERN;
    if (arp) {
        int total = style == STYLE_FARP ? 2 * n : n, len = (g.order == 2 || g.order == 3) && total > 1 ? 2 * total - 2 : total;
        uint32_t rng = 0x9e3779b9u;
        for (int k = 0; k * step < 192 && drawn < 64; ++k) {
            int idx = (int)umod((u32)k, (u32)len);
            if (idx >= total) idx = 2 * total - 2 - idx;
            if (g.order == 1 || g.order == 3) idx = total - 1 - idx;
            if (g.order == 5) idx = (int)art_below(&rng, (u32)total);
            int note = (int)umod((u32)idx, (u32)n), oct = (int)udiv((u32)idx, (u32)n);
            int t = k * step + ((k & 1) ? sdiv((g.swing - 50) * step, 100) : 0), L = sdiv(step * g.length, 100);
            if (t >= 192) break;
            int x = X0 + sdiv(W * t, 192), x2 = X0 + sdiv(W * (t + L), 192);
            if (x2 > X1) x2 = X1;
            if (x2 < x + 2) x2 = x + 2;
            int y = Y1 - (note + 1) * rowh - oct * rowh / 2;
            if (y < Y0 + 2) y = Y0 + 2;
            thick(thick_role(vc.n ? vc.role[note] : 0, g.quality), x * Q, y * Q, x2 * Q, y * Q);
            ++drawn;
        }
    } else {
        int hits = style == STYLE_CHORD && g.loop ? sdiv(192, step) : 1, every = style == STYLE_CHORD ? step : 192;
        int spread = style == STYLE_CHORD ? 0 : step;
        for (int h = 0; h < hits && drawn < 64; ++h)
            for (int i = 0; i < n && drawn < 64; ++i) {
                int t = h * every + ((h & 1) ? sdiv((g.swing - 50) * step, 100) : 0) + sdiv(spread * i, n);
                int L = sdiv((style == STYLE_CHORD && g.loop ? step : 192) * g.length, 100) - sdiv(spread * i, n);
                if (L < 3) L = 3;
                if (t >= 192) continue;
                int x = X0 + sdiv(W * t, 192), x2 = X0 + sdiv(W * (t + L), 192);
                if (x2 > X1) x2 = X1;
                if (x2 < x + 2) x2 = x + 2;
                int y = Y1 - (i + 1) * rowh;
                thick(thick_role(vc.n ? vc.role[i] : 0, g.quality), x * Q, y * Q, x2 * Q, y * Q);
                ++drawn;
            }
    }
    if (g.loop) use(LOOPMARK);
    if (view.stepping) place(PLAYHEAD, X0 + sdiv(W * (int)umod(view.pos * (u32)step, 192u), 192), 0, Q);
    for (int i = 0; i < 4; ++i) {
        int k = g.kinds[g.fl][i];
        icon(k, 122 + 34 * i, 192, k == g.quality ? 717 : 614);
        if (k == g.quality) icon_on(122 + 34 * i, 192, 717);
        place(CRITTER_AT + g.extsets[g.pf][i], 122 + 34 * (4 + i), 192, 640);
    }
    Line l;
    say(TX_T2L0, 10, 36, "loop");
    say(TX_T2V0, 10, 56, g.loop ? "on" : "off");
    say(TX_T2L1, 10, 146, "length");
    l.clear(); l.num(g.length); l.put("%"); say(TX_T2V1, 10, 166, l.s);
    say(TX_T2L2, 392, 36, "swing");
    l.clear(); l.num(g.swing); l.put("%"); say(TX_T2V2, 392, 56, l.s);
    say(TX_T2L3, 392, 146, "human");
    l.clear(); l.num(g.human); l.put("%"); say(TX_T2V3, 392, 166, l.s);
}
static void palette_page() {
    use(PAGE3);
    for (int q = 0; q < 4; ++q) {
        int ext = q >= 2, row = q & 1, y = row ? 148 : 50, live = ext ? row == g.pf : row == g.fl;
        for (int i = 0; i < 4; ++i) {
            int x = (ext ? 268 : 42) + 56 * i;
            if (!ext) {
                int k = g.kinds[row][i];
                icon(k, x, y, live ? 1331 : 1126);
                if (live && k == g.quality) icon_on(x, y, 1331);
                say(TX_T3L0 + q * 4 + i, x, y + 38, art_kind_names[k]);
            } else {
                int e = g.extsets[row][i];
                say(TX_T3L0 + q * 4 + i, x, y + 38, art_pollen_names[e]);
                if (live && ((g.extensions >> e) & 1u)) thin(THIN_HUE0 + e, (x - 10) * Q, (y + 16) * Q, (x + 10) * Q, (y + 16) * Q);
            }
        }
        use(FRAME_AT + q * 2 + (live ? 1 : 0));
        place(CURSOR_AT + q, (ext ? 268 : 42) + 56 * (cursor[q] & 3), y, Q);
    }
    for (int r = 0; r < 2; ++r)
        for (int i = 0; i < 4; ++i) place(CRITTER_AT + g.extsets[r][i], 268 + 56 * i, r ? 148 : 50, Q);
}
static void gardens_page(u32 ms, int shift) {
    use(PAGE4);
    int walking = view.walking;
    if (walking) use(WALKON);
    if (walking || shift) { use(WALKBOX); say(TX_T4WALK, 47, 28, "walk"); }
    else hush(TX_T4WALK);
    if (shift) {
        int any = 0;
        for (int i = 0; i < 8; ++i) if (planted(i)) any = 1;
        say(TX_T4S0, 90, 28, WALK_LEN_NAME[g.walk_len]);
        say(TX_T4S1, 425, 28, WALK_ORDER_NAME[g.walk_order]);
        say(TX_T4S2, 272, 28, walking ? "blue stops" : any ? "blue walks" : "plant first");
        hush(TX_T4TOP);
    } else {
        Line top;
        top.clear();
        top.put(NOTE_NAME[mod12(g.root)]); top.put(" "); top.put(art_kind_names[g.quality]);
        for (u32 e = 0; e < 12; ++e)
            if (((g.extensions >> e) & 1u) && !art_pollen_silent(g.quality, e) && top.n + 4 < 23) { top.put(" "); top.put(art_pollen_names[e]); }
        say(TX_T4TOP, 240, 24, top.s);
        hush(TX_T4S0); hush(TX_T4S1); hush(TX_T4S2);
    }
    static const char* const LABEL[8] = { "1 in", "2 out", "3", "4", "5", "6", "7 m1", "8 m2" };
    u32 small_used[12] = { 0 };
    for (int i = 0; i < 8; ++i) {
        int x0 = 14 + 116 * (i & 3), y0 = i < 4 ? 34 : 124;
        int set = planted(i);
        int match = set && mod12(g.pad_root[i]) == mod12(g.root) && g.seed_q[i] == g.quality && seed_mask(i) == g.extensions;
        use(GBOX_AT + i * 3 + (match ? 2 : set ? 1 : 0));
        if (i == g.pad_sel) place(GSEL, x0, y0, Q);
        if (set) {
            int bob = match ? wobble(ms, 500u, 33554u, 0, 2) : 0, sw = i == g.pad_sel ? wobble(ms, 3000u, 5592u, 0, 2) : 0;
            int iy = y0 + 28 - (bob < 0 ? -bob : bob);
            icon(g.seed_q[i], x0 + 10 + sw, iy, 614);
            if (match || i == g.pad_sel) icon_on(x0 + 10 + sw, iy, 614);
            Line nm;
            nm.clear(); nm.put(NOTE_NAME[mod12(g.pad_root[i])]); nm.put(" "); nm.put(art_kind_names[g.seed_q[i]]);
            say(TX_T4N0 + i, x0 + 60, y0 + 34, nm.s);
        } else hush(TX_T4N0 + i);
        say(TX_T4K0 + i, x0 + 52 - (i >= 2 && i <= 5 ? 9 : 0), y0 + 79, LABEL[i]);
        if (i >= 2 && i <= 5) use(MARK_AT + (i - 2) * 2 + (i == g.pad_sel ? 1 : 0));
        for (int k = 0; k < 4; ++k) {
            int qx = x0 + 6 + 24 * k, qy = y0 + 42, e = g.seed_ext[i][k];
            if (e < 12) {
                if (small_used[e] < (u32)SMALL_COPIES) place(SMALL_AT + e * SMALL_COPIES + (int)small_used[e]++, qx + 10, qy + 9, 563);
                else thick(THICK_HUE0 + e, (qx + 4) * Q, (qy + 9) * Q, (qx + 16) * Q, (qy + 9) * Q);
            }
            if (i == g.pad_sel && k == (g.square & 3)) place(GSQ, qx, qy, Q);
        }
        if (walking && i == (int)view.planter) {
            int run = (int)(((view.walk_q10 > 1024u ? 1024u : view.walk_q10) * 104u) >> 10);
            thin(THIN_DGRAY, x0 * Q, (y0 + 2) * Q, (x0 + 104) * Q, (y0 + 2) * Q);
            if (run > 1) thin(THIN_WHITE, x0 * Q, (y0 + 2) * Q, (x0 + run) * Q, (y0 + 2) * Q);
        }
    }
}
static void help_page(int shift) {
    use(HELP);
    static const char* const PAGE_NAME[4] = { "play", "rhythm", "palette", "gardens" };
    int pg = page;
    for (int k = 0; k < 4; ++k) {
        say(TX_HTW0 + k, 60 + 120 * k, 26, k == pg ? PAGE_NAME[k] : "");
        say(TX_HTG0 + k, 60 + 120 * k, 26, k == pg ? "" : PAGE_NAME[k]);
    }
    const char* turn[4] = { "", "", "", "" };
    const char* push[4] = { "", "", "", "" };
    int style = g.style, arp = style == STYLE_ARP || style == STYLE_FARP;
    if (pg == PAGE_PLAY) {
        turn[0] = shift ? "echo" : "cascade";
        turn[1] = shift ? "rate" : "style";
        turn[2] = style == STYLE_PATTERN && !shift ? "phrase" : style != STYLE_PATTERN && (arp ^ shift) ? "arp order" : "voicing";
        turn[3] = shift ? "boost pulse" : "boost";
        if (!shift) { push[0] = "push: voice leading"; push[1] = "push: chord banks"; push[2] = "push: hold"; push[3] = "push: extension banks"; }
    } else if (pg == PAGE_RHYTHM) {
        turn[0] = "loop"; turn[1] = "length"; turn[2] = "swing"; turn[3] = "human";
    } else if (pg == PAGE_PALETTE) {
        turn[0] = "chord bank a"; turn[1] = "chord bank b"; turn[2] = "extension bank a"; turn[3] = "extension bank b";
        if (shift) { push[0] = push[2] = "push: swap banks"; push[1] = push[3] = "push: reset"; }
        else for (int k = 0; k < 4; ++k) push[k] = "push: next unused";
    } else if (shift) {
        turn[0] = "walk length"; turn[1] = "walk order";
        push[0] = view.walking ? "push: stop the walk" : "push: walk";
    } else {
        turn[0] = "garden"; turn[1] = "root"; turn[2] = "kind"; turn[3] = "extension square";
        push[0] = "push: plant the chord"; push[3] = "push: next square";
    }
    for (int k = 0; k < 4; ++k) {
        say(TX_HR0 + k, 34, 63 + 30 * k, turn[k]);
        say(TX_HP0 + k, 472, 63 + 30 * k, push[k]);
    }
    say(TX_HK0, 8, 194, pg == PAGE_GARDENS ? "1-8 play the gardens" : "1-4 kind, 5-8 extension");
    say(TX_HK1, 8, 212, pg == PAGE_GARDENS ? "shift+1-8 plant there" : "tap keeps, hold returns");
}
static int texts_page = -1;
static int page_of_text(int t) {
    return t <= TX_T1_LAST ? PAGE_PLAY : t <= TX_T2_LAST ? PAGE_RHYTHM : t <= TX_T3_LAST ? PAGE_PALETTE :
           t <= TX_T4_LAST ? PAGE_GARDENS : PAGE_HELP;
}
static int page_drawn_now() { return help_open ? PAGE_HELP : page; }
static void dress(u32 ms, int shift) {
    for (int i = 0; i < 12; ++i) icon_used[i] = 0;
    icon_on_used = 0;
    int pg = page_drawn_now();
    if (pg != texts_page) {
        for (int t = 0; t < TEXT_COUNT; ++t) if (page_of_text(t) != pg) hush(t);
        texts_page = pg;
    }
    if (pg == PAGE_HELP) help_page(shift);
    else if (pg == PAGE_PLAY) play_page(ms, shift);
    else if (pg == PAGE_RHYTHM) rhythm_page();
    else if (pg == PAGE_PALETTE) palette_page();
    else gardens_page(ms, shift);
    commit();
}
enum { SAVE_FOLDER = 0, SAVE_QUIET_MS = 1500 };
static const char SAVE_NAME[] = "garden.bin";
static struct { u32 loaded, edits_seen, changed_ms, writes, failures; int read_result; } disk;
static void load_settings() {
    Settings s;
    int got = op1f_read(&op1f_card_garden, SAVE_FOLDER, SAVE_NAME, &s, sizeof s);
    disk.read_result = got;
    if (got != (int)sizeof s || !settings_valid(&s)) return;
    u32 m = mask_off();
    const unsigned char* src = (const unsigned char*)&s;
    unsigned char* dst = (unsigned char*)&g;
    for (u32 i = 0; i < sizeof g; ++i) dst[i] = src[i];
    mask_on(m);
}
static void keep_settings(const Op1fFrame* now) {
    if (!disk.loaded) { disk.loaded = 1; load_settings(); disk.edits_seen = edits; return; }
    if (edits != disk.edits_seen) { disk.edits_seen = edits; disk.changed_ms = now->ms ? now->ms : 1u; return; }
    if (!disk.changed_ms || now->ms - disk.changed_ms < (u32)SAVE_QUIET_MS || now->disk) return;
    Settings s;
    u32 m = mask_off();
    const unsigned char* src = (const unsigned char*)&g;
    unsigned char* dst = (unsigned char*)&s;
    for (u32 i = 0; i < sizeof g; ++i) dst[i] = src[i];
    mask_on(m);
    if (op1f_write(&op1f_card_garden, SAVE_FOLDER, SAVE_NAME, &s, sizeof s) == (int)sizeof s) ++disk.writes;
    else ++disk.failures;
    disk.changed_ms = 0;
}
static void part_name(char* out, int i) {
    static const int tens[4] = { 1000, 100, 10, 1 };
    int at = 1, started = 0;
    out[0] = 'n';
    for (int k = 0; k < 4; ++k) {
        int d = 0;
        while (i >= tens[k]) { i -= tens[k]; ++d; }
        if (d || started || k == 3) { out[at++] = (char)('0' + d); started = 1; }
    }
    out[at] = 0;
}
static struct {
    u32 asked, shown, frames, opened, closed, step, finding, stood, sig;
    int page_drawn, shift_drawn, countdown;
    u32 rate_ms, rate_ticks;
} ui;
static u32 state_sig() {
    u32 s = edits * 2654435761u;
    s ^= (u32)page_drawn_now() * 97u ^ keys_down * 31u ^ view.pos * 13u ^ view.beat16 * 7u ^ (u32)view.playing * 5u;
    s ^= view.walk_q10 * 3u ^ (u32)view.n * 11u ^ (u32)view.root * 17u ^ (u32)view.planter * 19u ^ (u32)synth_live() * 23u;
    for (int n = 0; n < 128; n += 4)
        s = s * 33u + (u32)(st.count[n] | (st.count[n + 1] << 2) | (st.count[n + 2] << 4) | (st.count[n + 3] << 6));
    return s;
}
extern "C" void garden_frame(const Op1fFrame* now) OP1F_NAMED("garden_frame");
extern "C" void garden_frame(const Op1fFrame* now) {
    if (now->bytes < 28u) return;
    u32 ms = now->ms;
    if (!ui.asked) {
        ui.asked = 1;
        u32 m = mask_off();
        if (!g.magic) defaults();
        mask_on(m);
        void* made = op1f_picture(&op1f_card_garden, "garden.svg");
        if (made) for (int i = 0; i < PART_COUNT; ++i) have[i] = 1;
        document = made;
        ui.page_drawn = -1;
    }
    if (now->bytes >= (u32)FRAME_L3) keep_settings(now);
    if (document && ui.finding < (u32)PART_COUNT) {
        char name[8];
        for (int k = 0; k < 64 && ui.finding < (u32)PART_COUNT; ++k, ++ui.finding) {
            part_name(name, (int)ui.finding);
            part[ui.finding] = op1f_node(document, name);
            if (part[ui.finding]) ++parts_found;
        }
        if (ui.finding >= (u32)PART_COUNT) {
            make_fonts();
            dress(ms - ms % 66u, 0);
            ui.sig = state_sig();
            page_ready = 1;
        }
    }
    u32 shown = now->bytes >= 32u && now->page == &op1f_card_garden ? 1u : 0u;
    if (shown && !ui.shown) { ++ui.opened; ui.page_drawn = -1; texts_page = -1; }
    if (!shown && ui.shown) ++ui.closed;
    if (!shown) help_open = 0;
    ui.shown = shown;
    page_shown = shown;
    if (shown) ++ui.frames;
    if (page_ready && !ui.stood && op1f_page_stand_in(&op1f_card_garden, ASK, document)) ui.stood = 1;
    if (page_ready && shown) {
        u32 step = ms - ms % 66u, sig = state_sig();
        int shift = now->shift != 0;
        int pg = page_drawn_now();
        int animated = pg == PAGE_PLAY || pg == PAGE_GARDENS;
        if (sig != ui.sig || pg != ui.page_drawn || shift != ui.shift_drawn || (animated && step != ui.step)) {
            ui.sig = sig;
            ui.page_drawn = pg;
            ui.shift_drawn = shift;
            ui.step = step;
            dress(step, shift);
        }
    }
}
