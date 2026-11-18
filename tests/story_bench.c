/* Story bench: runs "The Flood Line" with a human-like relay keeper and writes
 *   docs/narrative/play-order.json  scenes in the order they opened, per seed
 *   docs/narrative/ablation.json    story numbers on/off, one beat off, each choice path
 *   docs/narrative/tension.json     per-leg crises and failures, one row per night
 * Usage: story_bench <dir>          write all three files (make story)
 *        story_bench --check <dir>  re-run a few rows and require them verbatim in the files (make
 * test) story_bench --summary      print per-arm means only (tuning aid; writes nothing) Every run
 * is deterministic. */
#include "line_run.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef RunPlan Job;

static RunResult run_job(Job j)
{
    RunResult r;
    run_line(&j, &r);
    return r;
}

/* ------------------------------------------------------------------ rows */
static const char *ending_id(int e) { return e == 0 ? "end-ford" : e == 1 ? "end-siren" : "none"; }

static void play_row(char *buf, size_t cap, int i, const RunResult *r)
{
    const Choices ch = play_choices(i);
    int route = ch.route, ford = ch.ford;
    size_t n = (size_t)snprintf(
        buf, cap, "{\"seed\": \"%d\", \"choices\": {\"stones\": %d, \"ford\": %d}, \"order\": [", i,
        route, ford);
    for (int k = 0; k < r->order_len; k++) {
        n += (size_t)snprintf(buf + n, cap - n, "%s\"%s\"", k ? ", " : "", SCENE_ID[r->order[k]]);
    }
    n += (size_t)snprintf(buf + n, cap - n, "], \"won\": %s, \"ending\": \"%s\", \"quiet\": [",
                          r->completed ? "true" : "false", ending_id(r->ending));
    int first = 1;
    for (int k = 0; k < r->order_len; k++) {
        int s = r->order[k];
        if (line_heavy(s) && r->quiet[s]) {
            n += (size_t)snprintf(buf + n, cap - n, "%s\"%s\"", first ? "" : ", ", SCENE_ID[s]);
            first = 0;
        }
    }
    (void)snprintf(buf + n, cap - n, "]}");
}

static void ablation_row(char *buf, size_t cap, int i, const RunResult *r)
{
    (void)snprintf(buf, cap, "{\"seed\": \"%d\", \"seconds\": %.2f, \"fails\": %d, \"red\": %.2f}",
                   i, r->seconds, r->fails, r->red);
}

static void tension_row(char *buf, size_t cap, int i, int run, const RunResult *r)
{
    const Choices ch = tension_choices(run);
    int route = ch.route, ford = ch.ford;
    size_t n = (size_t)snprintf(buf, cap,
                                "{\"seed\": \"%d\", \"run\": %d, \"path\": {\"stones\": \"%d\", "
                                "\"ford\": \"%d\"}, \"completed\": %s, \"crisis\": [",
                                i, run, route, ford, r->completed ? "true" : "false");
    for (int k = 0; k < L_LEGS; k++) {
        n += (size_t)snprintf(buf + n, cap - n, "%s%d", k ? ", " : "",
                              r->failure[k] > 0 ? r->failure[k] : r->crisis[k]);
    }
    n += (size_t)snprintf(buf + n, cap - n, "], \"failure\": [");
    for (int k = 0; k < L_LEGS; k++) {
        n += (size_t)snprintf(buf + n, cap - n, "%s%d", k ? ", " : "", r->failure[k]);
    }
    (void)snprintf(buf + n, cap - n, "]}");
}

/* ------------------------------------------------------------------ arms */
enum { ARM_ON, ARM_OFF, ARM_HUT, ARM_BOARD, ARM_SPILL, ARM_CUT, ARM_MILL, ARM_FORD, ARMS };
static const unsigned ARM_EFFECTS[ARMS] = {
    LE_ALL, 0u,    LE_ALL & ~LE_HUT, LE_ALL & ~LE_BOARD, LE_ALL & ~LE_SPILL, LE_ALL & ~LE_CUT,
    LE_ALL, LE_ALL};
static const int ARM_ROUTE[ARMS] = {
    DEFAULT_ROUTE, DEFAULT_ROUTE, DEFAULT_ROUTE, DEFAULT_ROUTE, DEFAULT_ROUTE, DEFAULT_ROUTE, 0,
    DEFAULT_ROUTE};
static const int ARM_FORD_[ARMS] = {DEFAULT_FORD, DEFAULT_FORD, DEFAULT_FORD, DEFAULT_FORD,
                                    DEFAULT_FORD, DEFAULT_FORD, DEFAULT_FORD, 0};

static Job ablation_job(int arm, int i)
{
    return (Job){ablation_key(i), ARM_EFFECTS[arm], {ARM_ROUTE[arm], ARM_FORD_[arm]}};
}
static Job play_job(int i) { return (Job){play_key(i), LE_ALL, play_choices(i)}; }
static Job tension_job(int i, int run)
{
    return (Job){tension_key(i, run), LE_ALL, tension_choices(run)};
}

/* ------------------------------------------------------------------ full output */
static FILE *open_out(const char *dir, const char *name)
{
    char path[512];
    (void)snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "w");
    if (!f) {
        (void)fprintf(stderr, "cannot write %s\n", path);
        exit(1);
    }
    return f;
}

/* A short or failed write would leave a truncated table; stop with an error instead. */
static void close_out(FILE *f)
{
    int bad = ferror(f) != 0;
    if (fclose(f) != 0 || bad) {
        (void)fprintf(stderr, "write error\n");
        exit(1);
    }
}

static void write_rows(FILE *f, const char *indent, const RunResult *res, int arm)
{
    char row[256];
    for (int i = 0; i < ABLATION_SEEDS; i++) {
        ablation_row(row, sizeof row, i, &res[arm * ABLATION_SEEDS + i]);
        (void)fprintf(f, "%s%s%s\n", indent, row, i + 1 < ABLATION_SEEDS ? "," : "");
    }
}

static void write_arm_pair(FILE *f, const char *name, const RunResult *res, int off, int last)
{
    (void)fprintf(f, "    \"%s\": {\n      \"on\": [\n", name);
    write_rows(f, "        ", res, ARM_ON);
    (void)fprintf(f, "      ],\n      \"off\": [\n");
    write_rows(f, "        ", res, off);
    (void)fprintf(f, "      ]\n    }%s\n", last ? "" : ",");
}

static const char *GEN = "tests/story_bench.c (make story)";
/* Header texts. They are printed from the constants the code uses (line.h, line_policy.h), and
 * story_bench --check requires each of them verbatim in the files, so a text that drifts from the
 * code fails make test. */
static void metric_texts(char *buf, size_t cap)
{
    (void)snprintf(
        buf, cap,
        "  \"metrics\": {\n"
        "    \"seconds\": {\"better\": \"lower\", \"kind\": \"outcome\", \"felt\": "
        "\"screen\", \"label\": \"t on the night clock at first light (shown as the clock); "
        "every t is run through the solver (the held bells and a dry cell being cranked "
        "included, no flat additions)\"},\n"
        "    \"fails\": {\"better\": \"lower\", \"kind\": \"outcome\", \"felt\": \"screen\", "
        "\"label\": \"failures in the night: a turn lost and relaunched at the last mark (it "
        "ran off the posts, wandered %.0f t, or the cell went dry after the cut), a dry cell "
        "Kit has to crank, or a catch arm struck and the motor jammed; each raises a failure "
        "banner and counts on the Failures counter\"},\n"
        "    \"red\": {\"better\": \"lower\", \"kind\": \"resource\", \"felt\": \"resource\", "
        "\"label\": \"t run with the cell gauge in the red (charge below %.2f); a reading of "
        "the charge the beats change, not a play result\"}\n"
        "  },\n",
        LINE_LEG_LIMIT, LINE_RED_LINE);
}

static void tension_texts(char *buf, size_t cap)
{
    char policy[2048];
    keeper_describe(policy, sizeof policy);
    (void)snprintf(
        buf, cap,
        "  \"policy\": \"%s\",\n"
        "  \"crisis\": \"the cell gauge in the red (charge below %.2f) at any moment in the "
        "leg, or a failure (a failure counts as a crisis)\",\n"
        "  \"failure\": \"the cell ran dry (Kit cranks it, or after the cut the turn is "
        "lost), the turn ran off the posts or wandered "
        "%.0f t without a held mark and was relaunched, or it struck a catch arm too fast and "
        "jammed the motor\",\n",
        policy, LINE_RED_LINE, LINE_LEG_LIMIT);
}

static void run_all(RunResult *res, int np, int na)
{
    for (int i = 0; i < np; i++) {
        res[i] = run_job(play_job(i));
    }
    for (int a = 0; a < ARMS; a++) {
        for (int i = 0; i < ABLATION_SEEDS; i++) {
            res[np + a * ABLATION_SEEDS + i] = run_job(ablation_job(a, i));
        }
    }
    for (int i = 0; i < TENSION_SEEDS; i++) {
        for (int k = 0; k < TENSION_RUNS; k++) {
            res[np + na + i * TENSION_RUNS + k] = run_job(tension_job(i, k));
        }
    }
}

static int summary(void)
{
    static const char *const name[ARMS] = {"on",     "off",  "-hut", "-board",
                                           "-spill", "-cut", "mill", "ford"};
    for (int a = 0; a < ARMS; a++) {
        double s = 0, r = 0, f = 0;
        int done = 0;
        for (int i = 0; i < ABLATION_SEEDS; i++) {
            RunResult x = run_job(ablation_job(a, i));
            s += x.seconds;
            r += x.red;
            f += x.fails;
            done += x.completed;
        }
        printf("%-7s seconds %7.2f red %6.2f fails %5.2f done %d/%d\n", name[a], s / ABLATION_SEEDS,
               r / ABLATION_SEEDS, f / ABLATION_SEEDS, done, ABLATION_SEEDS);
    }
    double crisis[L_LEGS] = {0};
    int zero = 0, done = 0, n = TENSION_SEEDS * TENSION_RUNS;
    for (int i = 0; i < TENSION_SEEDS; i++) {
        for (int k = 0; k < TENSION_RUNS; k++) {
            RunResult x = run_job(tension_job(i, k));
            int fails = 0;
            for (int l = 0; l < L_LEGS; l++) {
                crisis[l] += x.failure[l] > 0 ? x.failure[l] : x.crisis[l];
                fails += x.failure[l];
            }
            zero += fails == 0;
            done += x.completed;
        }
    }
    printf("tension per leg:");
    for (int l = 0; l < L_LEGS; l++) {
        printf(" %.2f", crisis[l] / n);
    }
    printf("; no failure %d/%d; completed %d/%d\n", zero, n, done, n);
    return 0;
}

static int write_all(const char *dir)
{
    int np = PLAY_SEEDS, na = ARMS * ABLATION_SEEDS, nt = TENSION_SEEDS * TENSION_RUNS;
    RunResult *res = calloc((size_t)np + (size_t)na + (size_t)nt, sizeof *res);
    if (!res) {
        return 1;
    }
    run_all(res, np, na);
    char row[1024];

    FILE *f = open_out(dir, "play-order.json");
    (void)fprintf(
        f,
        "{\n  \"version\": 1,\n  \"generator\": \"%s\",\n  \"note\": \"Scenes open at fixed "
        "progress points: the start of the night and the end of a leg. "
        "stones 0 = the mill bell, 1 = the church bell; ford 0 = the ford bell, 1 = the hill "
        "siren. quiet lists the heavy scenes "
        "shown with no 'mark held' banner raised before the next frame of the line. Do not "
        "edit by hand.\",\n  \"seeds\": [\n",
        GEN);
    for (int i = 0; i < np; i++) {
        play_row(row, sizeof row, i, &res[i]);
        (void)fprintf(f, "    %s%s\n", row, i + 1 < np ? "," : "");
    }
    (void)fprintf(f, "  ]\n}\n");
    close_out(f);

    const RunResult *ab = res + np;
    f = open_out(dir, "ablation.json");
    (void)fprintf(
        f,
        "{\n  \"version\": 1,\n  \"generator\": \"%s\",\n  \"note\": \"Same %d seeds in every "
        "arm. on = all story numbers, the church bell and the hill siren; "
        "off = scenes still open but every story number is neutral; beats = only that scene's "
        "numbers removed; choices = one choice fixed per path, "
        "the other at its default. Do not edit by hand.\",\n",
        GEN, ABLATION_SEEDS);
    char head[4096];
    metric_texts(head, sizeof head);
    (void)fprintf(f, "%s  \"effects\": {\n    \"on\": [\n", head);
    write_rows(f, "      ", ab, ARM_ON);
    (void)fprintf(f, "    ],\n    \"off\": [\n");
    write_rows(f, "      ", ab, ARM_OFF);
    (void)fprintf(f, "    ]\n  },\n  \"beats\": {\n");
    write_arm_pair(f, "hut", ab, ARM_HUT, 0);
    write_arm_pair(f, "bell", ab, ARM_BOARD, 0);
    write_arm_pair(f, "spill", ab, ARM_SPILL, 0);
    write_arm_pair(f, "cut", ab, ARM_CUT, 1);
    (void)fprintf(f, "  },\n  \"choices\": {\n    \"stones\": {\"paths\": {\n      \"0\": [\n");
    write_rows(f, "        ", ab, ARM_MILL);
    (void)fprintf(f, "      ],\n      \"1\": [\n");
    write_rows(f, "        ", ab, ARM_ON);
    (void)fprintf(f, "      ]\n    }},\n    \"ford\": {\"paths\": {\n      \"0\": [\n");
    write_rows(f, "        ", ab, ARM_FORD);
    (void)fprintf(f, "      ],\n      \"1\": [\n");
    write_rows(f, "        ", ab, ARM_ON);
    (void)fprintf(f, "      ]\n    }}\n  }\n}\n");
    close_out(f);

    const RunResult *tn = res + np + na;
    f = open_out(dir, "tension.json");
    tension_texts(head, sizeof head);
    (void)fprintf(f, "{\n  \"version\": 1,\n  \"generator\": \"%s\",\n%s", GEN, head);
    (void)fprintf(
        f,
        "  \"segments\": [\"to the weir\", \"to the bell\", \"back to the gorge\", \"the water "
        "in the hut\", \"up to the dam road\"],\n"
        "  \"note\": \"%d seeds x %d nights; every night has its own stirred seed; the path "
        "comes from the night number.\",\n  \"seeds\": [\n",
        TENSION_SEEDS, TENSION_RUNS);
    for (int i = 0; i < nt; i++) {
        tension_row(row, sizeof row, i / TENSION_RUNS, i % TENSION_RUNS, &tn[i]);
        (void)fprintf(f, "    %s%s\n", row, i + 1 < nt ? "," : "");
    }
    (void)fprintf(f, "  ]\n}\n");
    close_out(f);

    double sec = 0;
    for (int i = 0; i < ABLATION_SEEDS; i++) {
        sec += ab[i].seconds;
    }
    printf("story bench: %d nights; mean night %.1f t (default path)\n", np + na + nt,
           sec / ABLATION_SEEDS);
    free(res);
    return 0;
}

/* ------------------------------------------------------------------ check mode */
static char *slurp(const char *dir, const char *name)
{
    char path[512];
    (void)snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "rb");
    if (!f) {
        (void)fprintf(stderr, "missing %s\n", path);
        exit(1);
    }
    /* Read in growing chunks; the bench files are tens of kB. */
    size_t cap = 65536, len = 0;
    char *buf = malloc(cap);
    if (!buf) {
        exit(1);
    }
    for (;;) {
        len += fread(buf + len, 1, cap - 1 - len, f);
        if (len < cap - 1) {
            break;
        }
        char *bigger = realloc(buf, cap * 2);
        if (!bigger) {
            exit(1);
        }
        buf = bigger;
        cap *= 2;
    }
    if (ferror(f)) {
        exit(1);
    }
    buf[len] = 0;
    (void)fclose(f); /* read only: nothing is lost if closing fails */
    return buf;
}

static int check(const char *dir)
{
    int failures = 0;
    /* [seed] every keeper key in the bench is distinct (paired arms share a seed by design). */
    int nk = PLAY_SEEDS + ABLATION_SEEDS + TENSION_SEEDS * TENSION_RUNS, k = 0;
    uint64_t *keys = malloc(sizeof *keys * (size_t)nk);
    if (!keys) {
        return 1;
    }
    for (int i = 0; i < PLAY_SEEDS; i++) {
        keys[k++] = play_key(i);
    }
    for (int i = 0; i < ABLATION_SEEDS; i++) {
        keys[k++] = ablation_key(i);
    }
    for (int i = 0; i < TENSION_SEEDS; i++) {
        for (int r = 0; r < TENSION_RUNS; r++) {
            keys[k++] = tension_key(i, r);
        }
    }
    for (int i = 1; i < nk; i++) {
        for (int j = 0; j < i; j++) {
            if (keys[i] != keys[j]) {
                continue;
            }
            (void)fputs("[seed] duplicate run key\n", stderr);
            failures++;
        }
    }
    free(keys);

    enum { CHECKS = 8 };
    Job jobs[CHECKS] = {play_job(0),
                        play_job(PLAY_SEEDS - 1),
                        ablation_job(ARM_ON, 0),
                        ablation_job(ARM_OFF, 0),
                        ablation_job(ARM_MILL, ABLATION_SEEDS - 1),
                        ablation_job(ARM_FORD, ABLATION_SEEDS - 1),
                        tension_job(0, 0),
                        tension_job(TENSION_SEEDS - 1, TENSION_RUNS - 1)};
    char *play = slurp(dir, "play-order.json"), *abl = slurp(dir, "ablation.json"),
         *ten = slurp(dir, "tension.json");
    char row[1024];
    /* How many times each re-run row must appear: the default arm is reused by every beat and as
     * one path of each choice. */
    static const int copies[CHECKS] = {1, 1, 7, 1, 1, 1, 1, 1};
    for (int i = 0; i < CHECKS; i++) {
        RunResult r = run_job(jobs[i]);
        const char *hay, *file;
        if (i < 2) {
            play_row(row, sizeof row, i ? PLAY_SEEDS - 1 : 0, &r);
            hay = play;
            file = "play-order";
        } else if (i < 6) {
            ablation_row(row, sizeof row, i < 4 ? 0 : ABLATION_SEEDS - 1, &r);
            hay = abl;
            file = "ablation";
        } else {
            tension_row(row, sizeof row, i == 6 ? 0 : TENSION_SEEDS - 1,
                        i == 6 ? 0 : TENSION_RUNS - 1, &r);
            hay = ten;
            file = "tension";
        }
        int found = 0;
        for (const char *c = hay; (c = strstr(c, row)); c++) {
            found++;
        }
        if (found != copies[i]) {
            (void)fprintf(stderr, "[bench] %s.json does not match the code (%d of %d copies): %s\n",
                          file, found, copies[i], row);
            failures++;
        }
    }
    /* [text] the descriptions in the files are the ones printed from the code's constants today. */
    char text[4096];
    metric_texts(text, sizeof text);
    if (!strstr(abl, text)) {
        (void)fputs("[text] ablation.json metric labels do not match the code's constants\n",
                    stderr);
        failures++;
    }
    tension_texts(text, sizeof text);
    if (!strstr(ten, text)) {
        (void)fputs("[text] tension.json policy/crisis/failure do not match the code's constants\n",
                    stderr);
        failures++;
    }
    free(play);
    free(abl);
    free(ten);
    if (failures) {
        (void)fprintf(stderr, "story bench check: %d failure(s); run make story to regenerate\n",
                      failures);
        return 1;
    }
    puts("story bench check: 8 re-run rows and the metric/policy texts match docs/narrative; all "
         "run seeds distinct");
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 3 && strcmp(argv[1], "--check") == 0) {
        return check(argv[2]);
    }
    if (argc == 2 && strcmp(argv[1], "--summary") == 0) {
        return summary();
    }
    if (argc == 2) {
        return write_all(argv[1]);
    }
    (void)fputs("usage: story_bench <dir> | story_bench --check <dir> | story_bench --summary\n",
                stderr);
    return 2;
}
