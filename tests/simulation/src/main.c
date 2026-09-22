// SPDX-License-Identifier: MIT
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <drivers/behavior.h>
#include <dt-bindings/zmk/keys.h> /* also read by app.overlay */
#include <zmk/hid.h>
#include <zmk/endpoints.h>

LOG_MODULE_REGISTER(zmk, CONFIG_ZMK_LOG_LEVEL);

/* Hardware boundary only: no Win-lock and no running Launcher macro. */
uint8_t macro_running;
bool get_fn_win_lock(void) { return false; }

static unsigned assertions;
static unsigned reports;
static unsigned known_defects;
static unsigned shift_first_defect_scenarios;
static unsigned release_masked_shift_scenarios;
static const char *scenario = "startup";
static struct zmk_hid_keyboard_report last_report;

#define CHECK(condition) do { \
    assertions++; \
    if (!(condition)) { \
        printf("FAIL %s line=%d: %s\n", scenario, __LINE__, #condition); \
        exit(1); \
    } \
} while (0)

/* Replaces physical endpoints.c. Records the real hid.c report, not a model. */
int zmk_endpoints_send_report(uint16_t page) {
    CHECK(page == HID_USAGE_KEY);
    last_report = *zmk_hid_get_keyboard_report();
    reports++;
    printf("REPORT %s %u", scenario, reports);
    const uint8_t *bytes = (const uint8_t *)&last_report;
    for (size_t i = 0; i < sizeof(last_report); i++) {
        printf(" %02x", bytes[i]);
    }
    printf("\n");
    return 0;
}

static void binding(const char *name, uint32_t code, uint32_t position, bool down) {
    struct zmk_behavior_binding b = {.behavior_dev = (char *)name, .param1 = code};
    struct zmk_behavior_binding_event e = {.position = position, .timestamp = k_uptime_get()};
    int err = down ? behavior_keymap_binding_pressed(&b, e)
                   : behavior_keymap_binding_released(&b, e);
    CHECK(err == 0);
}

static void kp(uint32_t code, bool down) { binding("SIM_KP", code, 100, down); }
static void morph(const char *name, bool down) { binding(name, 0, 1, down); }

static bool contains(const struct zmk_hid_keyboard_report *r, uint8_t usage) {
    for (size_t i = 0; i < sizeof(r->body.keys); i++) {
        if (r->body.keys[i] == usage) { return true; }
    }
    return false;
}

static void expect_one(uint8_t usage, uint8_t mods) {
    CHECK(last_report.report_id == 1);
    CHECK(last_report.body._reserved == 0);
    CHECK(last_report.body.modifiers == mods);
    unsigned count = 0;
    for (size_t i = 0; i < sizeof(last_report.body.keys); i++) {
        if (last_report.body.keys[i]) {
            count++;
            CHECK(last_report.body.keys[i] == usage);
        }
    }
    CHECK(count == 1);
}

static void idle(void) {
    const struct zmk_hid_keyboard_report *r = zmk_hid_get_keyboard_report();
    CHECK(r->body.modifiers == 0);
    CHECK(zmk_hid_get_explicit_mods() == 0);
    for (size_t i = 0; i < sizeof(r->body.keys); i++) { CHECK(r->body.keys[i] == 0); }
    CHECK(last_report.body.modifiers == 0);
    for (size_t i = 0; i < sizeof(last_report.body.keys); i++) {
        CHECK(last_report.body.keys[i] == 0);
    }
#if CONFIG_ADAPATIVE_NKRO
    extern struct zmk_adapative_nkro adapative_nkro;
    CHECK(adapative_nkro.kb_keys_count == 0);
    CHECK(adapative_nkro.nkro_bits_count == 0);
#endif
}

struct substitution_case {
    const char *id;
    const char *behavior;
    bool shifted;
    uint8_t usage; /* Independent numeric oracle; never derived from DTS. */
    uint8_t mods;
};

static const struct substitution_case cases[] = {
    {"C01", "SIM_GRAVE", true, 0x2e, 0x02},
    {"C02", "SIM_N2", true, 0x2f, 0x00},
    {"C03", "SIM_N6", true, 0x2e, 0x00},
    {"C04", "SIM_N7", true, 0x23, 0x02},
    {"C05", "SIM_N8", true, 0x34, 0x02},
    {"C06", "SIM_N9", true, 0x25, 0x02},
    {"C07", "SIM_N0", true, 0x26, 0x02},
    {"C08", "SIM_MINUS", true, 0x87, 0x02},
    {"C09", "SIM_EQUAL", false, 0x2d, 0x02},
    {"C10", "SIM_EQUAL", true, 0x33, 0x02},
    {"C11", "SIM_LB", false, 0x30, 0x00},
    {"C12", "SIM_LB", true, 0x30, 0x02},
    {"C13", "SIM_RB", false, 0x32, 0x00},
    {"C14", "SIM_RB", true, 0x32, 0x02},
    {"C15", "SIM_BACKSLASH", false, 0x87, 0x00},
    {"C16", "SIM_BACKSLASH", true, 0x89, 0x02},
    {"C17", "SIM_SEMI", true, 0x34, 0x00},
    {"C18", "SIM_QUOTE", false, 0x24, 0x02},
    {"C19", "SIM_QUOTE", true, 0x1f, 0x02},
    {"C20", "SIM_GRAVE", false, 0x2f, 0x02},
};

static unsigned substitutions(void) {
    unsigned count = 0;
    for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
        const struct substitution_case *c = &cases[i];
        /* shift0/1/2 = left, right, both physical Shift.
         * release0 = morph released first; release1 = physical Shift released first. */
        unsigned variants = c->shifted ? 6 : 1;
        for (unsigned v = 0; v < variants; v++) {
            char name[48];
            snprintf(name, sizeof(name), "%s-shift%u-release%u", c->id, v % 3, v / 3);
            scenario = name;
            bool left = c->shifted && v % 3 != 1;
            bool right = c->shifted && v % 3 != 0;
            uint8_t physical = (left ? 0x02 : 0) | (right ? 0x20 : 0);
            if (left) { kp(LSHFT, true); }
            if (right) { kp(RSHFT, true); }
            if (c->shifted) {
                /* The physical Shift itself is reported before the morph, so a
                 * mask leaking from a previous scenario would show up here. */
                CHECK(last_report.body.modifiers == physical);
                CHECK(zmk_hid_get_explicit_mods() == physical);
            }
            unsigned before = reports;
            morph(c->behavior, true);
            /* Exactly one report for the morph press event itself. This says
             * nothing about reports caused by other events in the scenario
             * (see the Shift-first release check below). */
            CHECK(reports == before + 1);
            expect_one(c->usage, c->mods);
            struct zmk_hid_keyboard_report held = last_report;
            before = reports;
            k_msleep(10);
            CHECK(reports == before); /* No artificial repeat taps. */
            CHECK(memcmp(&held, zmk_hid_get_keyboard_report(), sizeof(held)) == 0);
            if (v >= 3) {
                if (left) { kp(LSHFT, false); }
                if (right) { kp(RSHFT, false); }
                /* Characterization, not acceptance: releasing the physical
                 * Shift first drops the modifiers the substitution depends on
                 * while the substituted usage is still held. With mods == 0x02
                 * the host now sees the unshifted JIS key. Same root cause as
                 * KNOWN_DEFECT implicit-shift-lost-on-physical-shift-release. */
                CHECK(contains(&last_report, c->usage));
                CHECK(last_report.body.modifiers == 0);
                if (c->mods != 0) {
                    shift_first_defect_scenarios++;
                    printf("INTERMEDIATE_DEFECT %s modifiers=00 expected=%02x\n", name, c->mods);
                }
            }
            before = reports;
            morph(c->behavior, false);
            CHECK(reports == before + 1); /* One report for the morph release event. */
            CHECK(!contains(&last_report, c->usage));
            /* Characterization: Mod-Morph keeps its mask on the trigger Shift
             * until after the release report, and clearing the mask sends no
             * report. So with the physical Shift still held (v < 3) the host is
             * told Shift is up; HID state has it down again, unreported. See
             * KNOWN_DEFECT physical-shift-reported-released-on-morph-release. */
            CHECK(last_report.body.modifiers == 0);
            CHECK(zmk_hid_get_keyboard_report()->body.modifiers == (v < 3 ? physical : 0));
            if (c->shifted && v < 3) {
                release_masked_shift_scenarios++;
            }
            if (v < 3) {
                if (left) { kp(LSHFT, false); }
                if (right) { kp(RSHFT, false); }
            }
            idle();
            printf("PASS %s\n", name);
            count++;
        }
    }
    return count;
}

static void baseline(void) {
    scenario = "baseline-kp";
    kp(A, true);
    expect_one(0x04, 0);
    kp(A, false);
    kp(RSHFT, true);
    kp(N2, true);
    expect_one(0x1f, 0x20);
    kp(N2, false);
    kp(RSHFT, false);
    idle();
    scenario = "baseline-mod-morph-normal";
    morph("SIM_N2", true);
    expect_one(0x1f, 0);
    morph("SIM_N2", false);
    idle();
}

static void defect(const char *id) {
    known_defects++;
    printf("KNOWN_DEFECT %s\n", id);
}

static void baseline_defects(void) {
    scenario = "shift-released-before-transformed-key";
    kp(LSHFT, true);
    morph("SIM_MINUS", true);
    expect_one(0x87, 0x02);
    kp(LSHFT, false);
    expect_one(0x87, 0);
    defect("implicit-shift-lost-on-physical-shift-release");
    morph("SIM_MINUS", false);
    idle();

    scenario = "physical-shift-reported-released";
    kp(LSHFT, true);
    morph("SIM_MINUS", true);
    expect_one(0x87, 0x02);
    unsigned before_release = reports;
    morph("SIM_MINUS", false);
    CHECK(reports == before_release + 1);
    CHECK(last_report.body.modifiers == 0);               /* host: Shift up */
    CHECK(zmk_hid_get_keyboard_report()->body.modifiers == 0x02); /* state: down */
    kp(A, true);
    expect_one(0x04, 0x02);                                /* Shift reappears */
    kp(A, false);
    defect("physical-shift-reported-released-on-morph-release");
    kp(LSHFT, false);
    idle();

    scenario = "shift-after-key";
    /* Key first, Shift second: Mod-Morph already chose the normal binding, so
     * the later physical Shift applies to the unsubstituted usage. US "2" then
     * Shift shows Shift+2 on the JIS host (a double quote), not "@". */
    morph("SIM_N2", true);
    expect_one(0x1f, 0);
    kp(LSHFT, true);
    expect_one(0x1f, 0x02);
    defect("physical-shift-after-key-applies-to-unsubstituted-usage");
    morph("SIM_N2", false);
    kp(LSHFT, false);
    idle();

    scenario = "implicit-shift-overwritten";
    morph("SIM_EQUAL", true);
    expect_one(0x2d, 0x02);
    kp(A, true);
    CHECK(contains(&last_report, 0x2d));
    CHECK(last_report.body.modifiers == 0);
    kp(A, false);
    CHECK(last_report.body.modifiers == 0);
    defect("implicit-shift-overwritten-by-unrelated-key");
    morph("SIM_EQUAL", false);
    idle();

    scenario = "two-masks-one-release";
    kp(LSHFT, true);
    morph("SIM_N2", true);
    morph("SIM_N6", true);
    CHECK(last_report.body.modifiers == 0);
    morph("SIM_N6", false);
    /* Mod-Morph clears the shared mask after the key-up report. */
    CHECK(zmk_hid_get_keyboard_report()->body.modifiers == 0x02);
    CHECK(last_report.body.modifiers == 0);
    kp(A, true);
    CHECK(contains(&last_report, 0x2f));
    CHECK(last_report.body.modifiers == 0x02);
    defect("mask-cleared-while-other-morph-held");
    kp(A, false);
    morph("SIM_N2", false);
    kp(LSHFT, false);
    idle();

    scenario = "output-usage-collision";
    kp(MINUS, true);
    unsigned before = reports;
    morph("SIM_EQUAL", true);
    CHECK(reports == before + 2); /* Synthetic pre-release, then press. */
    kp(MINUS, false);
    CHECK(!contains(&last_report, 0x2d));
    defect("shared-output-released-while-equal-held");
    morph("SIM_EQUAL", false);
    idle();

    scenario = "same-mod-morph-twice";
    morph("SIM_N2", true);
    struct zmk_behavior_binding b = {.behavior_dev = "SIM_N2"};
    struct zmk_behavior_binding_event e = {.position = 2, .timestamp = k_uptime_get()};
    CHECK(behavior_keymap_binding_pressed(&b, e) == -ENOTSUP);
    defect("same-mod-morph-instance-second-press-rejected");
    morph("SIM_N2", false);
    idle();
}

static void long_hold(void) {
    scenario = "long-hold-no-taps";
    morph("SIM_BACKSLASH", true);
    expect_one(0x87, 0);
    unsigned before = reports;
    k_msleep(1000);
    CHECK(reports == before);
    CHECK(contains(zmk_hid_get_keyboard_report(), 0x87));
    morph("SIM_BACKSLASH", false);
    idle();
    printf("PASS long-hold-no-taps\n");
}

static void international_overflow(void) {
#if CONFIG_ADAPATIVE_NKRO
    extern struct zmk_adapative_nkro adapative_nkro;
    scenario = "international-adaptive-overflow";
    const uint32_t keys[] = {A, B, C, D, E, F};
    for (size_t i = 0; i < ARRAY_SIZE(keys); i++) { kp(keys[i], true); }
    /* Positive control: a seventh ordinary usage (G = 0x0a <= NKRO max F20)
     * does go into the NKRO bitmap, so the overflow path below is exercised. */
    kp(G, true);
    CHECK(zmk_hid_keyboard_is_pressed(0x0a));
    CHECK(adapative_nkro.nkro_bits_count == 1);
    kp(G, false);
    CHECK(adapative_nkro.nkro_bits_count == 0);
    /* International1 (0x87) exceeds the bitmap; hid.c returns -EINVAL from
     * select_keyboard_usage but zmk_hid_keyboard_press discards it and returns
     * 0, so the press API reports success while the key is silently dropped.
     * kp() asserting err == 0 is therefore part of the characterization. */
    kp(INTERNATIONAL_1, true);
    CHECK(!zmk_hid_keyboard_is_pressed(0x87));
    CHECK(!contains(&last_report, 0x87));
    defect("international1-dropped-after-six-keys");
    kp(INTERNATIONAL_1, false);
    for (size_t i = 0; i < ARRAY_SIZE(keys); i++) { kp(keys[i], false); }
    idle();
#endif
}

void main(void) {
    /* Line-buffer stdout so a killed (timed out) run still leaves its log. */
    setvbuf(stdout, NULL, _IOLBF, 0);
    printf("SIMULATION pinned-Keychron-ZMK native_posix_64 adaptive=%d\n",
           IS_ENABLED(CONFIG_ADAPATIVE_NKRO));
    baseline();
    unsigned count = substitutions();
    long_hold();
    baseline_defects();
    international_overflow();
    printf("SUMMARY substitutions=%u shift_first_defect_scenarios=%u "
           "release_masked_shift_scenarios=%u known_defects=%u assertions=%u reports=%u\n",
           count, shift_first_defect_scenarios, release_masked_shift_scenarios, known_defects,
           assertions, reports);
    printf("SIMULATION_PASS\n");
    exit(0);
}
