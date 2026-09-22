// SPDX-License-Identifier: MIT
/* US-JIS substituter tests (spec section 10) on the real pinned hid.c,
 * hid_listener.c and event manager, with this repository's module linked in.
 * Input goes through the real behavior API. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <drivers/behavior.h>
#include <dt-bindings/zmk/keys.h>
#include <dt-bindings/zmk/usjis.h>
#include <zmk/hid.h>
#include <zmk/endpoints.h>
#include <zmk/event_manager.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/usjis.h>

LOG_MODULE_REGISTER(zmk, CONFIG_ZMK_LOG_LEVEL);

/* Hardware boundary only: no Win-lock and no running Launcher macro. */
uint8_t macro_running;
bool get_fn_win_lock(void) { return false; }
/* keymap.c is not compiled: the active layers are set by the test. Layer 2 is
 * the Win base layer that the OS slide switch holds (CONFIG_ZMK_USJIS_LAYER_MASK). */
static uint32_t layer_state = 1 << 2;
uint32_t zmk_keymap_layer_state(void) { return layer_state; }

static unsigned assertions;
static unsigned scenarios;
static unsigned total_reports;
static unsigned consumer_reports;
static const char *scenario = "startup";

#define CHECK(condition) do { \
    assertions++; \
    if (!(condition)) { \
        printf("FAIL %s line=%d: %s\n", scenario, __LINE__, #condition); \
        exit(1); \
    } \
} while (0)

/* Keyboard reports recorded since the start of the scenario. */
struct rep { uint8_t mods; uint8_t keys[6]; };
#define MAX_REPS 64
static struct rep reps[MAX_REPS];
static unsigned nreps;

/* Replaces physical endpoints.c. Records the real hid.c report. */
int zmk_endpoints_send_report(uint16_t page) {
    if (page == HID_USAGE_CONSUMER) {
        consumer_reports++;
        printf("CONSUMER_REPORT %s\n", scenario);
        return 0;
    }
    CHECK(page == HID_USAGE_KEY);
    const struct zmk_hid_keyboard_report *r = zmk_hid_get_keyboard_report();
    CHECK(r->report_id == 1);
    CHECK(r->body._reserved == 0);
    CHECK(nreps < MAX_REPS);
    reps[nreps].mods = r->body.modifiers;
    memcpy(reps[nreps].keys, r->body.keys, 6);
    nreps++;
    total_reports++;
    printf("REPORT %s %u %02x [", scenario, nreps, r->body.modifiers);
    for (int i = 0; i < 6; i++) {
        if (r->body.keys[i]) { printf(" %02x", r->body.keys[i]); }
    }
    printf(" ]\n");
    return 0;
}

static void start(const char *name) {
    scenario = name;
    nreps = 0;
    scenarios++;
}

static void binding(const char *name, uint32_t code, uint32_t position, bool down) {
    struct zmk_behavior_binding b = {.behavior_dev = (char *)name, .param1 = code};
    struct zmk_behavior_binding_event e = {.position = position, .timestamp = k_uptime_get()};
    int err = down ? behavior_keymap_binding_pressed(&b, e)
                   : behavior_keymap_binding_released(&b, e);
    CHECK(err >= 0);
}

static void kp(uint32_t code, bool down) { binding("SIM_KP", code, 100, down); }
static void mode(uint32_t op) { binding("USJIS", op, 1, true); binding("USJIS", op, 1, false); }

/* A report equals {mods, [usages]} regardless of slot order. */
static bool rep_is(const struct rep *r, uint8_t mods, const uint8_t *usages, unsigned n) {
    if (r->mods != mods) { return false; }
    unsigned found = 0;
    for (int i = 0; i < 6; i++) {
        if (r->keys[i] == 0) { continue; }
        bool ok = false;
        for (unsigned j = 0; j < n; j++) { if (usages[j] == r->keys[i]) { ok = true; } }
        if (!ok) { return false; }
        found++;
    }
    return found == n;
}

static void dump(void) {
    for (unsigned i = 0; i < nreps; i++) {
        printf("  recorded %u: %02x [", i + 1, reps[i].mods);
        for (int k = 0; k < 6; k++) { if (reps[i].keys[k]) { printf(" %02x", reps[i].keys[k]); } }
        printf(" ]\n");
    }
}

/* Expect report number `index` (1-based) to be {mods,[usages]}. */
#define EXPECT(index, mods, ...) do { \
    const uint8_t u_[] = {0, ##__VA_ARGS__}; \
    unsigned n_ = sizeof(u_) - 1; \
    assertions++; \
    if ((index) > nreps || !rep_is(&reps[(index) - 1], (mods), u_ + 1, n_)) { \
        printf("FAIL %s line=%d: report %u expected %02x with %u usages\n", scenario, __LINE__, \
               (unsigned)(index), (unsigned)(mods), n_); \
        dump(); \
        exit(1); \
    } \
} while (0)

#define EXPECT_COUNT(n) do { \
    assertions++; \
    if (nreps != (unsigned)(n)) { \
        printf("FAIL %s line=%d: expected %u reports, got %u\n", scenario, __LINE__, (unsigned)(n), nreps); \
        dump(); \
        exit(1); \
    } \
} while (0)

static bool state_has(uint8_t usage) {
    const struct zmk_hid_keyboard_report *r = zmk_hid_get_keyboard_report();
    for (int i = 0; i < 6; i++) { if (r->body.keys[i] == usage) { return true; } }
    return false;
}

static void idle(void) {
    const struct zmk_hid_keyboard_report *r = zmk_hid_get_keyboard_report();
    CHECK(r->body.modifiers == 0);
    CHECK(zmk_hid_get_explicit_mods() == 0);
    for (int i = 0; i < 6; i++) { CHECK(r->body.keys[i] == 0); }
    CHECK(zmk_usjis_active_entries() == 0);
    CHECK(!zmk_usjis_is_pending());
    printf("PASS %s\n", scenario);
}

/* -------------------------------------------------------------- table */

struct row { const char *id; uint32_t input; bool shifted; uint8_t out; uint8_t mods; };
/* Independent numeric oracle from the spec table (not derived from the resolver). */
static const struct row rows[] = {
    {"C01", GRAVE, true, 0x2e, 0x02}, {"C02", N2, true, 0x2f, 0x00},
    {"C03", N6, true, 0x2e, 0x00},    {"C04", N7, true, 0x23, 0x02},
    {"C05", N8, true, 0x34, 0x02},    {"C06", N9, true, 0x25, 0x02},
    {"C07", N0, true, 0x26, 0x02},    {"C08", MINUS, true, 0x87, 0x02},
    {"C09", EQUAL, false, 0x2d, 0x02}, {"C10", EQUAL, true, 0x33, 0x02},
    {"C11", LBKT, false, 0x30, 0x00}, {"C12", LBKT, true, 0x30, 0x02},
    {"C13", RBKT, false, 0x32, 0x00}, {"C14", RBKT, true, 0x32, 0x02},
    {"C15", BSLH, false, 0x87, 0x00}, {"C16", BSLH, true, 0x89, 0x02},
    {"C17", SEMI, true, 0x34, 0x00},  {"C18", SQT, false, 0x24, 0x02},
    {"C19", SQT, true, 0x1f, 0x02},   {"C20", GRAVE, false, 0x2f, 0x02},
};

static unsigned table_tests(void) {
    unsigned count = 0;
    for (size_t i = 0; i < ARRAY_SIZE(rows); i++) {
        const struct row *c = &rows[i];
        /* shift0/1/2 = left, right, both physical Shift; release0 = key
         * released first; release1 = physical Shift released first. */
        unsigned variants = c->shifted ? 6 : 1;
        for (unsigned v = 0; v < variants; v++) {
            char name[48];
            snprintf(name, sizeof(name), "%s-shift%u-release%u", c->id, v % 3, v / 3);
            start(name);
            bool left = c->shifted && v % 3 != 1;
            bool right = c->shifted && v % 3 != 0;
            uint8_t physical = (left ? 0x02 : 0) | (right ? 0x20 : 0);
            if (left) { kp(LSHFT, true); }
            if (right) { kp(RSHFT, true); }
            unsigned before = nreps;
            kp(c->input, true);
            CHECK(nreps == before + 1);
            EXPECT(nreps, c->mods, c->out);
            /* Long press: the usage stays down, no extra reports (S06). */
            k_msleep(20);
            CHECK(nreps == before + 1);
            CHECK(state_has(c->out));
            if (v >= 3) {
                /* Physical Shift released first: the substituted output keeps
                 * its own modifiers (S14). */
                if (left) { kp(LSHFT, false); }
                if (right) { kp(RSHFT, false); }
                EXPECT(nreps, c->mods, c->out);
            }
            before = nreps;
            kp(c->input, false);
            CHECK(nreps == before + 1);
            /* Key up and the restored physical modifiers in one report. */
            EXPECT(nreps, v < 3 ? physical : 0);
            if (v < 3) {
                if (left) { kp(LSHFT, false); }
                if (right) { kp(RSHFT, false); }
            }
            idle();
            count++;
        }
    }

    /* Shift not in the table is non-substituted: unshifted rows with Shift
     * that the table does not list, and shifted-only rows without Shift. */
    start("table-nonmatching-2-without-shift");
    kp(N2, true);
    EXPECT(1, 0x00, 0x1f);
    kp(N2, false);
    EXPECT(2, 0x00);
    idle();

    /* Shift from the key's own binding matches (C10 via &kp PLUS). */
    start("table-binding-shift-plus");
    kp(PLUS, true);
    EXPECT(1, 0x02, 0x33);
    kp(PLUS, false);
    EXPECT(2, 0x00);
    idle();

    /* Ctrl is kept and not used for matching. */
    start("table-ctrl-kept-c09");
    kp(LCTRL, true);
    kp(EQUAL, true);
    EXPECT(2, 0x03, 0x2d);
    kp(EQUAL, false);
    EXPECT(3, 0x01);
    kp(LCTRL, false);
    idle();
    start("table-ctrl-kept-c02");
    kp(LCTRL, true);
    kp(RSHFT, true);
    kp(N2, true);
    EXPECT(3, 0x01, 0x2f);
    kp(N2, false);
    EXPECT(4, 0x21);
    kp(RSHFT, false);
    kp(LCTRL, false);
    idle();
    return count;
}

/* -------------------------------------------------------------- S tests */

static void s01_disabled_identity(void) {
    start("S01-disabled-identity");
    CHECK(!zmk_usjis_is_enabled());
    for (size_t i = 0; i < ARRAY_SIZE(rows); i++) {
        const struct row *c = &rows[i];
        nreps = 0;
        if (c->shifted) { kp(RSHFT, true); }
        kp(c->input, true);
        EXPECT(nreps, c->shifted ? 0x20 : 0x00, ZMK_HID_USAGE_ID(c->input));
        kp(c->input, false);
        if (c->shifted) { kp(RSHFT, false); }
        CHECK(zmk_hid_get_keyboard_report()->body.modifiers == 0);
    }
    idle();
}

static void s02(void) {
    start("S02");
    kp(RSHFT, true); kp(N2, true); kp(N2, false); kp(RSHFT, false);
    EXPECT_COUNT(4);
    EXPECT(1, 0x20); EXPECT(2, 0x00, 0x2f); EXPECT(3, 0x20); EXPECT(4, 0x00);
    idle();
}

static void s03(void) {
    start("S03");
    kp(RSHFT, true); kp(N2, true); kp(RSHFT, false); kp(N2, false);
    EXPECT_COUNT(4);
    EXPECT(1, 0x20); EXPECT(2, 0x00, 0x2f); EXPECT(3, 0x00, 0x2f); EXPECT(4, 0x00);
    idle();
}

static void s04(void) {
    start("S04");
    kp(EQUAL, true);
    EXPECT(1, 0x02, 0x2d);
    mode(USJIS_OFF);
    CHECK(zmk_usjis_is_enabled());
    CHECK(zmk_usjis_is_pending());
    kp(EQUAL, false);
    EXPECT(2, 0x00);
    CHECK(!zmk_usjis_is_enabled());
    CHECK(!zmk_usjis_is_pending());
    /* Now disabled: EQUAL is identity. */
    kp(EQUAL, true);
    EXPECT(3, 0x00, 0x2e);
    kp(EQUAL, false);
    mode(USJIS_ON);
    CHECK(zmk_usjis_is_enabled());
    idle();
}

static void s05(void) {
    start("S05");
    kp(RSHFT, true); kp(N2, true); kp(MINUS, true); kp(MINUS, false); kp(N2, false); kp(RSHFT, false);
    EXPECT_COUNT(6);
    EXPECT(1, 0x20); EXPECT(2, 0x00, 0x2f); EXPECT(3, 0x02, 0x2f, 0x87);
    EXPECT(4, 0x00, 0x2f); EXPECT(5, 0x20); EXPECT(6, 0x00);
    idle();
}

static void s06(void) {
    start("S06-long-hold");
    kp(BSLH, true);
    EXPECT(1, 0x00, 0x87);
    k_msleep(1000);
    EXPECT_COUNT(1);
    CHECK(state_has(0x87));
    kp(BSLH, false);
    idle();
}

static void s07(void) {
    for (int enabled = 0; enabled < 2; enabled++) {
        start(enabled ? "S07-enabled-consumer" : "S07-disabled-consumer");
        if (!enabled) { mode(USJIS_OFF); }
        unsigned before = consumer_reports;
        kp(C_VOL_UP, true);
        CHECK(consumer_reports == before + 1);
        CHECK(zmk_hid_consumer_is_pressed(0xe9));
        kp(C_VOL_UP, false);
        CHECK(consumer_reports == before + 2);
        EXPECT_COUNT(0);
        if (!enabled) { mode(USJIS_ON); }
        idle();
    }
}

static void s08(void) {
    for (int enabled = 0; enabled < 2; enabled++) {
        start(enabled ? "S08-enabled-mode-key" : "S08-disabled-mode-key");
        unsigned before = consumer_reports;
        mode(USJIS_TOG);
        mode(USJIS_TOG);
        EXPECT_COUNT(0);
        CHECK(consumer_reports == before);
        CHECK(zmk_usjis_is_enabled());
        idle();
    }
}

static void s11(void) {
    start("S11");
    kp(RSHFT, true); kp(N2, true); kp(A, true); kp(A, false); kp(N2, false); kp(RSHFT, false);
    EXPECT_COUNT(6);
    EXPECT(1, 0x20); EXPECT(2, 0x00, 0x2f); EXPECT(3, 0x20, 0x2f, 0x04);
    EXPECT(4, 0x00, 0x2f); EXPECT(5, 0x20); EXPECT(6, 0x00);
    idle();
}

static void s12(void) {
    start("S12");
    kp(EQUAL, true); kp(A, true); kp(A, false); kp(EQUAL, false);
    EXPECT_COUNT(4);
    EXPECT(1, 0x02, 0x2d); EXPECT(2, 0x00, 0x2d, 0x04); EXPECT(3, 0x02, 0x2d); EXPECT(4, 0x00);
    idle();
}

static void s13(void) {
    start("S13");
    kp(EQUAL, true); kp(RSHFT, true); kp(N2, true); kp(N2, false); kp(RSHFT, false); kp(EQUAL, false);
    EXPECT_COUNT(6);
    EXPECT(1, 0x02, 0x2d); EXPECT(2, 0x02, 0x2d); EXPECT(3, 0x00, 0x2d, 0x2f);
    EXPECT(4, 0x02, 0x2d); EXPECT(5, 0x02, 0x2d); EXPECT(6, 0x00);
    idle();
}

static void s14(void) {
    start("S14");
    kp(RSHFT, true); kp(MINUS, true); kp(RSHFT, false); kp(MINUS, false);
    EXPECT_COUNT(4);
    EXPECT(1, 0x20); EXPECT(2, 0x02, 0x87); EXPECT(3, 0x02, 0x87); EXPECT(4, 0x00);
    idle();
}

static void s15(void) {
    start("S15");
    mode(USJIS_OFF);
    CHECK(!zmk_usjis_is_enabled());
    kp(A, true);
    mode(USJIS_ON);
    CHECK(zmk_usjis_is_pending());
    kp(RSHFT, true); kp(N2, true);
    EXPECT(3, 0x20, 0x04, 0x1f); /* identity: still DISABLED */
    kp(N2, false);
    kp(A, false);
    CHECK(zmk_usjis_is_enabled());
    kp(N2, true);
    EXPECT(6, 0x00, 0x2f); /* C02 */
    kp(N2, false);
    kp(RSHFT, false);
    idle();
}

static void s16(void) {
    start("S16");
    unsigned writes = zmk_usjis_settings_writes();
    kp(EQUAL, true);
    mode(USJIS_TOG);
    CHECK(zmk_usjis_is_pending());
    mode(USJIS_TOG);
    CHECK(!zmk_usjis_is_pending());
    kp(EQUAL, false);
    CHECK(zmk_usjis_is_enabled());
    CHECK(zmk_usjis_settings_writes() == writes);
    kp(EQUAL, true);
    EXPECT(3, 0x02, 0x2d);
    kp(EQUAL, false);
    idle();
}

static void s17(void) {
    start("S17-endpoint-clear");
    kp(EQUAL, true);
    EXPECT(1, 0x02, 0x2d);
    /* What endpoints.c does on a switch away from a live transport. */
    zmk_hid_keyboard_clear();
    zmk_hid_consumer_clear();
    zmk_endpoints_send_report(HID_USAGE_KEY);
    zmk_endpoints_send_report(HID_USAGE_CONSUMER);
    EXPECT(2, 0x00);
    ZMK_EVENT_RAISE(new_zmk_endpoint_changed(
        (struct zmk_endpoint_changed){.endpoint = {.transport = ZMK_TRANSPORT_USB}}));
    CHECK(zmk_usjis_active_entries() == 1);
    kp(RSHFT, true); kp(A, true);
    EXPECT(4, 0x20, 0x04); /* uppercase A, no C09 output and no added Shift */
    kp(A, false); kp(RSHFT, false);
    unsigned before = nreps;
    kp(EQUAL, false);
    CHECK(nreps == before); /* no release report for the cleared record */
    idle();
}

static void s18(void) {
    start("S18");
    kp(LS(A), true); kp(EQUAL, true); kp(EQUAL, false); kp(LS(A), false);
    EXPECT_COUNT(4);
    EXPECT(1, 0x02, 0x04); EXPECT(2, 0x02, 0x04, 0x2d); EXPECT(3, 0x00, 0x04); EXPECT(4, 0x00);
    idle();
}

static void s19(void) {
    start("S19");
    kp(A, true); kp(EQUAL, true);
    EXPECT(2, 0x02, 0x04, 0x2d);
    kp(A, false);
    EXPECT(3, 0x02, 0x2d);
    unsigned before = nreps, cons = consumer_reports;
    kp(C_VOL_UP, true); kp(C_VOL_UP, false);
    CHECK(consumer_reports == cons + 2);
    CHECK(nreps == before); /* keyboard modifiers unchanged: no keyboard report */
    for (unsigned i = 2; i <= nreps; i++) { CHECK(reps[i - 1].mods == 0x02); }
    kp(EQUAL, false);
    EXPECT(nreps, 0x00);
    idle();
}

static void s20(void) {
    start("S20");
    kp(EQUAL, true);
    EXPECT(1, 0x02, 0x2d);
    kp(LC(X), true);
    EXPECT(2, 0x01, 0x2d, 0x1b);
    kp(A, true);
    EXPECT(3, 0x00, 0x2d, 0x1b, 0x04);
    kp(A, false);
    EXPECT(4, 0x00, 0x2d, 0x1b); /* Ctrl of X not restored */
    kp(LALT, true);
    EXPECT(5, 0x04, 0x2d, 0x1b);
    kp(LALT, false);
    EXPECT(6, 0x00, 0x2d, 0x1b);
    kp(LC(X), false);
    EXPECT(7, 0x02, 0x2d);
    kp(EQUAL, false);
    EXPECT(8, 0x00);
    idle();
}

/* Output usage shared by C09 (`=` -> 0x2d) and the physical MINUS key. */
static void shared_output(void) {
    start("shared-output-minus-then-c09");
    kp(MINUS, true);
    EXPECT(1, 0x00, 0x2d);
    kp(EQUAL, true);
    /* Pre-release of 0x2d (rule 4), then the press with Shift. */
    EXPECT(2, 0x00); EXPECT(3, 0x02, 0x2d);
    kp(MINUS, false);
    /* MINUS's record goes, C09 still owns 0x2d. */
    EXPECT(4, 0x02, 0x2d);
    CHECK(state_has(0x2d));
    kp(EQUAL, false);
    EXPECT(5, 0x00);
    idle();

    start("shared-output-c09-then-minus");
    kp(EQUAL, true);
    EXPECT(1, 0x02, 0x2d);
    kp(MINUS, true);
    EXPECT(2, 0x02); EXPECT(3, 0x00, 0x2d);
    kp(EQUAL, false);
    EXPECT(4, 0x00, 0x2d); /* MINUS still holds 0x2d; no substituted key left */
    CHECK(state_has(0x2d));
    kp(MINUS, false);
    EXPECT(5, 0x00);
    idle();
}

/* Key repeat re-sends the same press: the earlier record is replaced. */
static void repeated_press(void) {
    start("same-usage-pressed-twice");
    kp(RSHFT, true);
    kp(N2, true);
    EXPECT(2, 0x00, 0x2f);
    kp(N2, true);
    kp(N2, false);
    kp(RSHFT, false);
    idle();
}

/* Off the configured layers (the Mac position of the OS switch) the mode
 * stays ENABLED but nothing is substituted. */
static void layer_mask(void) {
    start("layer-mask-mac-identity");
    layer_state = 1 << 0;
    kp(RSHFT, true); kp(N2, true);
    EXPECT(2, 0x20, 0x1f);
    kp(N2, false); kp(RSHFT, false);
    CHECK(zmk_usjis_is_enabled());
    idle();
    start("layer-mask-win-fn-layer");
    layer_state = (1 << 2) | (1 << 3);
    kp(RSHFT, true); kp(N2, true);
    EXPECT(2, 0x00, 0x2f);
    kp(N2, false); kp(RSHFT, false);
    idle();
    layer_state = 1 << 2;
}

/* Two substitutable keys released in reverse order leave nothing behind. */
static void reverse_release(void) {
    start("reverse-release");
    kp(LBKT, true); kp(RBKT, true);
    EXPECT(2, 0x00, 0x30, 0x32);
    kp(LBKT, false);
    EXPECT(3, 0x00, 0x32);
    kp(RBKT, false);
    EXPECT(4, 0x00);
    idle();
}

void main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    printf("USJIS_SIMULATION pinned-Keychron-ZMK native_posix_64\n");
    scenario = "startup";
    CHECK(zmk_usjis_listener_order_ok());
    CHECK(!zmk_usjis_is_enabled()); /* DISABLED by default (S10 boot condition) */
    s01_disabled_identity();
    mode(USJIS_ON);
    CHECK(zmk_usjis_is_enabled());
    unsigned table = table_tests();
    s02(); s03(); s04(); s05(); s06(); s07(); s08();
    s11(); s12(); s13(); s14(); s15(); s16(); s17(); s18(); s19(); s20();
    shared_output();
    repeated_press();
    reverse_release();
    layer_mask();
    printf("SUMMARY usjis table=%u scenarios=%u assertions=%u reports=%u consumer_reports=%u\n",
           table, scenarios, assertions, total_reports, consumer_reports);
    printf("USJIS_SIMULATION_PASS\n");
    exit(0);
}
