/* "The Flood Line" story layer: scene order, the numbers each story beat changes, quiet heavy scenes, choices. */
#include "line_run.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static const int DESIGN[LS_COUNT] = {LS_HUT, LS_STONES, LS_BELL, LS_SPILL, LS_FORD, LS_CUT, LS_DAWN};

static int close_to(double a, double b) { return fabs(a - b) <= 1e-9 * fmax(1.0, fabs(b)); }

/* Run with the keeper, answering scenes with the given choices, until `target` opens. Returns frames run. */
static int drive(Line *l, Keeper *p, int target, int route, int ford)
{
    int frames = 0;
    while (l->scene != target) {
        assert(l->leg < L_LEGS && frames < RUN_GUARD_FRAMES);
        if (l->scene != LS_NONE) {
            assert(line_answer(l, l->scene == LS_STONES ? route : l->scene == LS_FORD ? ford : 0) == 0);
            continue;
        }
        double t = l->time;
        line_tick(l, 1, keeper_command(p, l));
        assert(fabs(l->time - t - 0.2) < 1e-9);  /* every frame is 0.2 t through the solver: no flat waits */
        frames++;
    }
    return frames;
}

static void start(Line *l, Keeper *p, unsigned effects)
{
    line_init(l, effects);
    keeper_init(p, play_key(0));
    assert(l->ready && l->scene == LS_HUT && line_options(LS_HUT) == 1);
}

static char *slurp(const char *path)
{
    static char text[2][1 << 17];
    static int slot;
    FILE *f = fopen(path, "rb");
    assert(f);
    char *buf = text[slot++ & 1];
    size_t len = fread(buf, 1, sizeof text[0] - 1, f);
    fclose(f);
    buf[len] = 0;
    return buf;
}

/* Two-sided sign test over the seeds: p for the larger of the two one-way counts. */
static double sign_p(int k, int n)
{
    double tail = 0.0;
    for (int x = k; x <= n; x++) {
        double comb = 1.0;
        for (int y = 0; y < x; y++) comb = comb * (n - y) / (y + 1);
        tail += comb;
    }
    return fmin(1.0, 2.0 * tail / pow(2.0, n));
}

/* Reads two row lists (seconds, fails, red per seed) that follow the heads `a` then `b` after `from`. */
static void read_rows(const char *at, const char *a, const char *b, double v[2][ABLATION_SEEDS][2])
{
    for (int arm = 0; arm < 2; arm++) {
        at = strstr(at, arm ? b : a);
        assert(at);
        for (int i = 0; i < ABLATION_SEEDS; i++) {
            at = strstr(at, "\"seconds\": ");
            /* The two outcomes: seconds and fails. red is a resource (the charge the beats change) and is not read here. */
            int fails;
            double red;
            assert(at && sscanf(at, "\"seconds\": %lf, \"fails\": %d, \"red\": %lf", &v[arm][i][0], &fails, &red) == 3);
            v[arm][i][1] = fails;
            at++;
        }
    }
}

static int measurable(double v[2][ABLATION_SEEDS][2], int m, int *side)
{
    int lower0 = 0, lower1 = 0;
    double diff = 0.0, base = 0.0;
    for (int i = 0; i < ABLATION_SEEDS; i++) {
        lower0 += v[0][i][m] < v[1][i][m];
        lower1 += v[1][i][m] < v[0][i][m];
        diff += v[0][i][m] - v[1][i][m];
        base += v[1][i][m];
    }
    double p = sign_p(lower0 > lower1 ? lower0 : lower1, ABLATION_SEEDS);
    double rel = fabs(diff / ABLATION_SEEDS) / fmax(fabs(base / ABLATION_SEEDS), 1.0);
    *side = lower0 > lower1 ? 0 : 1;
    return p <= 0.05 && rel >= 0.05;
}

/* One choice's two paths: each wins one outcome, and neither is better or equal on both in 85 % of seeds. */
static void check_choice(const char *text, const char *head)
{
    static double v[2][ABLATION_SEEDS][2];
    read_rows(strstr(text, head), "\"0\": [", "\"1\": [", v);
    int wins[2] = {0, 0}, side;
    for (int m = 0; m < 2; m++) if (measurable(v, m, &side)) wins[side]++;
    int dominated[2] = {0, 0};
    for (int i = 0; i < ABLATION_SEEDS; i++)
        for (int hi = 0; hi < 2; hi++) {
            const double *x = v[hi][i], *y = v[1 - hi][i];
            dominated[hi] += x[0] <= y[0] && x[1] <= y[1] && (x[0] < y[0] || x[1] < y[1]);
        }
    assert(wins[0] == 1 && wins[1] == 1);
    assert(dominated[0] < 0.85 * ABLATION_SEEDS && dominated[1] < 0.85 * ABLATION_SEEDS);
    printf("[choice] %s each path wins one outcome; better-or-equal on both in %d/%d and %d/%d seeds\n",
           head, dominated[0], ABLATION_SEEDS, dominated[1], ABLATION_SEEDS);
}

/* Removing a reward makes both outcomes worse; removing a punishment/loss makes both better. */
static void check_beat(const char *text, const char *beat, int expected_side)
{
    static double v[2][ABLATION_SEEDS][2];
    char head[64];
    snprintf(head, sizeof head, "\"%s\": {", beat);
    const char *at = strstr(strstr(text, "\"beats\": {"), head);
    assert(at);
    read_rows(at, "\"on\": [", "\"off\": [", v);
    int side, hits = 0;
    for (int m = 0; m < 2; m++) {
        assert(measurable(v, m, &side) && side == expected_side);
        double diff = 0.0;
        for (int i = 0; i < ABLATION_SEEDS; i++) diff += v[0][i][m] - v[1][i][m];
        assert(expected_side == 0 ? diff < 0.0 : diff > 0.0);
        hits++;
    }
    assert(hits == 2);
    printf("[beat] %s removed: %d of 2 outcomes differ measurably\n", beat, hits);
}

int main(void)
{
    /* [order] the bench's play-order.json (re-checked against the code by story_bench --check): every seed meets the
     * scenes in the designed order (setups before payoffs, the bottom before the end) and both endings are reached. */
    {
        const char *text = slurp("docs/narrative/play-order.json");
        char want[256] = "\"order\": [";
        for (int k = 0; k < LS_COUNT; k++) {
            strcat(want, k ? ", \"" : "\"");
            strcat(want, SCENE_ID[DESIGN[k]]);
            strcat(want, "\"");
        }
        strcat(want, "], \"won\": true");
        int seeds = 0, orders = 0;
        for (const char *c = text; (c = strstr(c, "\"seed\": ")); c++) seeds++;
        for (const char *c = text; (c = strstr(c, want)); c++) orders++;
        assert(seeds >= 8 && orders == seeds);
        static const char all[] = "\"quiet\": [\"spill\", \"ford\", \"cut\"]";
        assert(strstr(text, "\"ending\": \"end-ford\", \"quiet\": [\"spill\", \"ford\", \"cut\"]"));
        assert(strstr(text, "\"ending\": \"end-siren\", \"quiet\": [\"spill\", \"ford\", \"cut\"]"));
        for (const char *c = text; (c = strstr(c, "\"quiet\": [")); c++) assert(strncmp(c, all, sizeof all - 1) == 0);
        printf("[order] %d seeds: hut .. dawn in the designed order; both endings reached\n", seeds);
        RunResult r;
        run_line(play_key(3), LE_ALL, 1, 0, &r);
        assert(r.completed && r.order_len == LS_COUNT && r.ending == 0 && r.quiet[LS_SPILL] && r.quiet[LS_FORD] && r.quiet[LS_CUT]);
        for (int k = 0; k < LS_COUNT; k++) assert(r.order[k] == DESIGN[k]);
    }

    /* [mech] hut: Ada winds the cell past the line (0.10 -> 0.45), and Kit's call steadies the speed needle (x0.10). */
    {
        Line a, b; Keeper p;
        start(&a, &p, LE_ALL); start(&b, &p, LE_ALL & ~LE_HUT);
        assert(close_to(a.charge, 0.10) && close_to(b.charge, 0.10));
        assert(line_answer(&a, 0) == 0 && line_answer(&b, 0) == 0);
        assert(close_to(a.charge, 0.45) && close_to(b.charge, 0.10) && a.count == 1 && b.count == 1);
        line_tick(&a, 3, LC_COAST); line_tick(&b, 3, LC_COAST);
        assert(fabs(line_wobble(&b)) > 1e-3 && close_to(line_wobble(&a) / line_wobble(&b), 0.10));
        assert(a.target_x == 18.0 && a.from_x == 0.0);
        line_free(&a); line_free(&b);
        puts("[mech] hut: cell 0.10 -> 0.45; Kit's call: needle wobble x0.10");
    }

    /* [mech] stones: the mill bell is a short span (mark 30); the church a long one (42), and once its bell rings the sexton
     * cranks 0.20 back up the line. With no route effect the bell sits at 36 and nothing comes back. */
    {
        Line a, b, c; Keeper p, q, s;
        start(&a, &p, LE_ALL); start(&b, &q, LE_ALL); start(&c, &s, LE_ALL & ~LE_ROUTE);
        drive(&a, &p, LS_STONES, 0, 1); drive(&b, &q, LS_STONES, 1, 1); drive(&c, &s, LS_STONES, 1, 1);
        assert(line_options(LS_STONES) == 2 && !line_heavy(LS_STONES));
        assert(line_answer(&a, 0) == 0 && line_answer(&b, 1) == 0 && line_answer(&c, 1) == 0);
        assert(a.route == 0 && b.route == 1);
        assert(close_to(a.target_x, 30.0) && close_to(b.target_x, 42.0) && close_to(c.target_x, 36.0));
        /* Put each turn at rest on its mark and let the arm take it: only the church path gains charge, and it waits
         * 8 t on the mark while the sexton cranks. */
        assert(close_to(line_hold_need(&a), 0.0) && close_to(line_hold_need(&b), 8.0) && close_to(line_hold_need(&c), 0.0));
        Line *all[3] = {&a, &b, &c};
        for (int k = 0; k < 3; k++) {
            Line *l = all[k];
            l->field.eps = 0.0;
            field_set_kink(&l->field, l->target_x, 0.0, 1);
            l->x = l->target_x;
            l->charge = 0.5;
            l->drift = 0.0;
            while (l->scene == LS_NONE) line_tick(l, 1, LC_COAST);
            assert(l->scene == LS_BELL);
        }
        assert(close_to(a.charge, 0.5) && close_to(b.charge, 0.70) && close_to(c.charge, 0.5));
        line_free(&a); line_free(&b); line_free(&c);
        puts("[mech] stones: mill mark 30, nothing back; church mark 42 and +0.20 while its bell rings for 8 t");
    }

    /* [mech] bell: Kit's board cell, +0.45, goes into the line; then the spill scene opens.
     * [quiet] spill: the bell is a reward and keeps its "mark held" banner, but the spill that follows gets the screen to
     * itself: the banner is gone when the spill opens and stays gone after it closes. */
    {
        Line a, b; Keeper p, q;
        start(&a, &p, LE_ALL); start(&b, &q, LE_ALL & ~LE_BOARD);
        drive(&a, &p, LS_BELL, 1, 1); drive(&b, &q, LS_BELL, 1, 1);
        assert(a.banner == LB_HELD && a.banner_left > 0.0 && !line_heavy(LS_BELL));
        double fa = a.charge, fb = b.charge;
        int loud = a.loud;
        assert(line_answer(&a, 0) == 0 && line_answer(&b, 0) == 0);
        assert(close_to(a.charge - fa, 0.45) && close_to(b.charge, fb) && a.scene == LS_SPILL && b.scene == LS_SPILL);
        assert(line_heavy(LS_SPILL) && a.banner == LB_NONE && a.banner_left == 0.0);
        assert(line_answer(&a, 0) == 0);
        line_tick(&a, 1, keeper_command(&p, &a));
        assert(a.loud == loud && a.banner != LB_HELD);
        line_free(&a); line_free(&b);
        puts("[mech] bell: +0.45 from Kit's board cell");
        puts("[quiet] spill: shown with no mark-held banner, before or after");
    }

    /* [mech] spill: in leg 4 only, the wet cell bleeds 0.014 per t, running or not, and the needle shakes more. */
    {
        Line a, b; Keeper p, q;
        start(&a, &p, LE_ALL); start(&b, &q, LE_ALL & ~LE_SPILL);
        drive(&a, &p, LS_SPILL, 1, 1); drive(&b, &q, LS_SPILL, 1, 1);
        assert(line_answer(&a, 0) == 0 && line_answer(&b, 0) == 0);
        assert(a.leg == 2 && line_leak(&a) == 0.0);  /* leg 3, back to the gorge, is still dry */
        drive(&a, &p, LS_FORD, 1, 1); drive(&b, &q, LS_FORD, 1, 1);
        assert(line_answer(&a, 1) == 0 && line_answer(&b, 1) == 0);
        assert(a.leg == 3 && b.leg == 3);
        assert(close_to(line_leak(&a), 0.014) && line_leak(&b) == 0.0);
        a.field.eps = 0.0;
        double f = a.charge;
        line_tick(&a, 1, LC_COAST);
        assert(a.field.eps == 0.0 && fabs((f - a.charge) - 0.014 * 0.2) < 1e-9);
        b.time = a.time;
        assert(fabs(line_wobble(&a)) > fabs(line_wobble(&b)));
        line_free(&a); line_free(&b);
        puts("[mech] spill: leg 4 bleeds 0.014 per t and the needle shakes more");
    }

    /* [mech] ford, both paths. The ford bell: leg 4 goes past the water line to mark 49; a turn that stands on it starts
     * the bell, the ferry house's dry cell is already connected, and the leg ends after 10 t of ringing.
     * A turn that meets the low arm faster than 0.15 jams the motor once: a failure and 8 t dead. The siren: mark 32,
     * nothing more. [quiet] ford: no "mark held" banner when the call comes, nor after. */
    for (int ford = 0; ford < 2; ford++) {
        Line a; Keeper p;
        start(&a, &p, LE_ALL);
        drive(&a, &p, LS_FORD, 1, ford);
        assert(line_options(LS_FORD) == 2 && line_heavy(LS_FORD) && a.banner == LB_NONE);
        int loud = a.loud;
        double before_cell = a.charge;
        assert(line_answer(&a, ford) == 0);
        assert(close_to(a.charge - before_cell, ford ? 0.0 : 0.40));
        assert(close_to(line_leak(&a), ford ? 0.014 : 0.0));
        assert(a.ford == ford && close_to(a.target_x, ford ? 32.0 : 49.0));
        assert(close_to(line_hold_need(&a), ford ? 0.0 : 10.0));
        line_tick(&a, 1, keeper_command(&p, &a));
        assert(a.loud == loud && a.banner != LB_HELD);
        if (ford == 0) {
            /* A fast turn at the bell: the arm is struck. */
            a.field.eps = 0.0;
            field_set_kink(&a.field, 48.4, 0.3, 1);
            a.x = 48.4;
            a.charge = 0.2;
            int fails = a.fails;
            for (int k = 0; k < 5 && !a.knocked; k++) line_tick(&a, 1, LC_COAST);
            assert(a.knocked && a.fails == fails + 1 && a.fails_leg[3] == 1 && a.knocks == 1 && a.fails == a.lost + a.dry + a.knocks && a.banner == LB_KNOCK && a.drift > 0.0);
            /* A turn at rest on the bell: it rings without another cell credit, and 10 t later the leg ends. */
            field_set_kink(&a.field, 49.0, 0.0, 1);
            a.x = 49.0;
            a.drift = 0.0;
            double f = a.charge;
            while (a.held == 0.0) { f = a.charge; line_tick(&a, 1, LC_COAST); }
            assert(close_to(a.charge, f) && line_leak(&a) == 0.0);
            double t = a.time;
            while (a.scene == LS_NONE) line_tick(&a, 1, LC_COAST);
            assert(a.scene == LS_CUT && a.time - t >= 10.0 - 0.2 - 1e-9);
            puts("[quiet] ford: shown with no mark-held banner");
        }
        line_free(&a);
    }
    puts("[mech] ford: the ford bell = mark 49, +0.40 and no bleeding from connection; it rings for 10 t, a fast turn jams the arm; "
         "the siren = mark 32, nothing more");

    /* [mech] cut: once the lower posts are gone, a dry cell has no crank: the turn is lost, Ada winds a fresh one at the
     * last mark and the cell is topped up to 0.30; before, Kit cranks it in place with 0.15 after 8 t dead.
     * [quiet] cut: no banner when the posts go, nor after. */
    {
        Line a, b; Keeper p, q;
        start(&a, &p, LE_ALL); start(&b, &q, LE_ALL & ~LE_CUT);
        drive(&a, &p, LS_CUT, 1, 1); drive(&b, &q, LS_CUT, 1, 1);
        assert(line_heavy(LS_CUT) && a.banner == LB_NONE && b.banner == LB_NONE);
        int loud = a.loud;
        assert(line_answer(&a, 0) == 0 && line_answer(&b, 0) == 0);
        assert(a.count == 0 && b.count == 1 && a.leg == 4);
        line_tick(&a, 1, LC_COAST);
        assert(a.loud == loud && a.banner != LB_HELD);
        puts("[quiet] cut: shown with no mark-held banner");
        assert(close_to(line_arm_limit(&a), 0.04) && close_to(line_arm_limit(&b), 0.30));
        assert(close_to(line_torque(&a), 0.052) && close_to(line_torque(&b), 0.08));
        a.charge = b.charge = 0.0001;
        a.field.eps = -0.052; b.field.eps = -0.08;
        double from = a.from_x;
        line_tick(&a, 1, LC_DOWN); line_tick(&b, 1, LC_DOWN);
        assert(a.fails_leg[4] == 1 && a.lost == 1 && a.fails == a.lost + a.dry + a.knocks && close_to(a.x, from) && close_to(a.charge, 0.30) && a.drift > 7.9 && a.banner == LB_LOST);
        assert(b.fails_leg[4] == 1 && b.dry == 1 && b.fails == b.lost + b.dry + b.knocks && b.drift > 7.9 && b.charge == 0.0 && b.banner == LB_DRY);
        while (b.drift > 0.0) line_tick(&b, 1, LC_COAST);
        assert(b.charge > 0.14 && b.charge <= 0.15);  /* the motor is still spinning down as the crank charge goes in */
        line_free(&a); line_free(&b);
        puts("[mech] cut: torque x0.65 on the slack drum; a dry cell now loses the turn; Ada winds a fresh one at the last mark");
    }

    /* [choice] both paths of each choice trade one measured outcome for the other in the bench's ablation.json, and
     * [beat] every tagged beat changes both outcomes in the expected direction when only its numbers are removed. */
    {
        const char *text = slurp("docs/narrative/ablation.json");
        check_choice(text, "\"stones\": {\"paths\"");
        check_choice(text, "\"ford\": {\"paths\"");
        check_beat(text, "hut", 0);
        check_beat(text, "bell", 0);
        check_beat(text, "spill", 1);
        check_beat(text, "cut", 1);
    }

    /* [text] the constants the bench prints into docs/narrative are the ones the line runs on: one frame is LINE_FRAME t,
     * the red line is LINE_RED_LINE, and a leg wandered past LINE_LEG_LIMIT t loses the turn. */
    {
        Line a; Keeper p;
        start(&a, &p, LE_ALL);
        assert(line_answer(&a, 0) == 0);
        double t0 = a.time;
        line_tick(&a, 1, LC_COAST);
        assert(close_to(a.time - t0, LINE_FRAME));
        assert(line_value(&a, LV_RED_LINE) == LINE_RED_LINE);
        a.leg_time = LINE_LEG_LIMIT - 0.5 * LINE_FRAME;
        int lost = a.lost;
        line_tick(&a, 1, LC_COAST);
        assert(a.lost == lost + 1 && a.banner == LB_LOST);
        line_free(&a);
        puts("[text] frame, red line and leg limit are the constants the bench descriptions print");
    }

    /* An ordinary held mark is loud, so the quiet checks above are not vacuous; out-of-range answers are refused. */
    {
        Line a; Keeper p;
        start(&a, &p, LE_ALL);
        drive(&a, &p, LS_STONES, 0, 0);
        assert(a.banner == LB_HELD && a.loud == 1);
        assert(line_answer(&a, 2) == 1 && line_answer(&a, -1) == 1);
        line_tick(&a, -1, LC_DOWN);
        assert(a.scene == LS_STONES);
        line_free(&a);
    }
    puts("line tests passed");
    return 0;
}
