/* Story bench: runs "The Flood Line" with a human-like relay keeper and writes
 *   docs/narrative/play-order.json  scenes in the order they opened, per seed
 *   docs/narrative/ablation.json    story numbers on/off, one beat off, each choice path
 *   docs/narrative/tension.json     per-leg crises and failures, one row per night
 * Usage: story_bench <dir>          write all three files (make story)
 *        story_bench --check <dir>  re-run a few rows and require them verbatim in the files (make test)
 *        story_bench --summary      print per-arm means only (tuning aid; writes nothing)
 * Every run is deterministic. */
#include "line_run.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { uint64_t key; unsigned effects; int route, ford; } Job;

static RunResult run_job(Job j)
{
    RunResult r;
    run_line(j.key, j.effects, j.route, j.ford, &r);
    return r;
}

/* ------------------------------------------------------------------ rows */
static const char *ending_id(int e) { return e == 0 ? "end-ford" : e == 1 ? "end-siren" : "none"; }

static void play_row(char *buf, size_t cap, int i, const RunResult *r)
{
    int route, ford;
    play_choices(i, &route, &ford);
    size_t n = (size_t)snprintf(buf, cap, "{\"seed\": \"%d\", \"choices\": {\"stones\": %d, \"ford\": %d}, \"order\": [", i, route, ford);
    for (int k = 0; k < r->order_len; k++) n += (size_t)snprintf(buf + n, cap - n, "%s\"%s\"", k ? ", " : "", SCENE_ID[r->order[k]]);
    n += (size_t)snprintf(buf + n, cap - n, "], \"won\": %s, \"ending\": \"%s\", \"quiet\": [", r->completed ? "true" : "false", ending_id(r->ending));
    int first = 1;
    for (int k = 0; k < r->order_len; k++) {
        int s = r->order[k];
        if (line_heavy(s) && r->quiet[s]) { n += (size_t)snprintf(buf + n, cap - n, "%s\"%s\"", first ? "" : ", ", SCENE_ID[s]); first = 0; }
    }
    snprintf(buf + n, cap - n, "]}");
}

static void ablation_row(char *buf, size_t cap, int i, const RunResult *r)
{
    snprintf(buf, cap, "{\"seed\": \"%d\", \"seconds\": %.2f, \"red\": %.2f}", i, r->seconds, r->red);
}

static void tension_row(char *buf, size_t cap, int i, int run, const RunResult *r)
{
    int route, ford;
    tension_choices(run, &route, &ford);
    size_t n = (size_t)snprintf(buf, cap, "{\"seed\": \"%d\", \"run\": %d, \"path\": {\"stones\": \"%d\", \"ford\": \"%d\"}, \"completed\": %s, \"crisis\": [",
                                i, run, route, ford, r->completed ? "true" : "false");
    for (int k = 0; k < L_LEGS; k++) n += (size_t)snprintf(buf + n, cap - n, "%s%d", k ? ", " : "", r->failure[k] > 0 ? r->failure[k] : r->crisis[k]);
    n += (size_t)snprintf(buf + n, cap - n, "], \"failure\": [");
    for (int k = 0; k < L_LEGS; k++) n += (size_t)snprintf(buf + n, cap - n, "%s%d", k ? ", " : "", r->failure[k]);
    snprintf(buf + n, cap - n, "]}");
}

/* ------------------------------------------------------------------ arms */
enum { ARM_ON, ARM_OFF, ARM_HUT, ARM_BOARD, ARM_SPILL, ARM_CUT, ARM_MILL, ARM_FORD, ARMS };
static const unsigned ARM_EFFECTS[ARMS] = {
    LE_ALL, 0u, LE_ALL & ~LE_HUT, LE_ALL & ~LE_BOARD, LE_ALL & ~LE_SPILL, LE_ALL & ~LE_CUT, LE_ALL, LE_ALL};
static const int ARM_ROUTE[ARMS] = {DEFAULT_ROUTE, DEFAULT_ROUTE, DEFAULT_ROUTE, DEFAULT_ROUTE, DEFAULT_ROUTE, DEFAULT_ROUTE, 0, DEFAULT_ROUTE};
static const int ARM_FORD_[ARMS] = {DEFAULT_FORD, DEFAULT_FORD, DEFAULT_FORD, DEFAULT_FORD, DEFAULT_FORD, DEFAULT_FORD, DEFAULT_FORD, 0};

static Job ablation_job(int arm, int i) { return (Job){ablation_key(i), ARM_EFFECTS[arm], ARM_ROUTE[arm], ARM_FORD_[arm]}; }
static Job play_job(int i) { int r, f; play_choices(i, &r, &f); return (Job){play_key(i), LE_ALL, r, f}; }
static Job tension_job(int i, int run) { int r, f; tension_choices(run, &r, &f); return (Job){tension_key(i, run), LE_ALL, r, f}; }

/* ------------------------------------------------------------------ full output */
static FILE *open_out(const char *dir, const char *name)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "w");
    if (!f) { fprintf(stderr, "cannot write %s\n", path); exit(1); }
    return f;
}

static void write_rows(FILE *f, const char *indent, const RunResult *res, int arm)
{
    char row[256];
    for (int i = 0; i < ABLATION_SEEDS; i++) {
        ablation_row(row, sizeof row, i, &res[arm * ABLATION_SEEDS + i]);
        fprintf(f, "%s%s%s\n", indent, row, i + 1 < ABLATION_SEEDS ? "," : "");
    }
}

static void write_arm_pair(FILE *f, const char *name, const RunResult *res, int off, int last)
{
    fprintf(f, "    \"%s\": {\n      \"on\": [\n", name);
    write_rows(f, "        ", res, ARM_ON);
    fprintf(f, "      ],\n      \"off\": [\n");
    write_rows(f, "        ", res, off);
    fprintf(f, "      ]\n    }%s\n", last ? "" : ",");
}

static const char *GEN = "tests/story_bench.c (make story)";
static const char *POLICY =
    "A relay keeper who reads the gauges, not the code: runs the motor towards the mark until the speed readout reaches "
    "0.28-0.52 c, coasts, and brakes when a rule-of-thumb stopping distance worked out from the wobbling readout reaches "
    "the mark (25 % late to 30 % early), 0-3 frames slow; it stops braking when the readout falls below 0.01-0.05 c, "
    "then watches the position marker for 0.8-1.6 t: on the mark and still, it waits for the catch arm; still but off "
    "the mark, it plans a slow second span; moving, it brakes again. While Kit calls the brake it brakes on her word. "
    "On the ford bell it cuts the motor and waits. Its hand comes from the night's own stirred seed, re-drawn per leg. "
    "Choices follow the path field.";

static void run_all(RunResult *res, int np, int na)
{
    for (int i = 0; i < np; i++) res[i] = run_job(play_job(i));
    for (int a = 0; a < ARMS; a++)
        for (int i = 0; i < ABLATION_SEEDS; i++) res[np + a * ABLATION_SEEDS + i] = run_job(ablation_job(a, i));
    for (int i = 0; i < TENSION_SEEDS; i++)
        for (int k = 0; k < TENSION_RUNS; k++) res[np + na + i * TENSION_RUNS + k] = run_job(tension_job(i, k));
}

static int summary(void)
{
    static const char *const name[ARMS] = {"on", "off", "-hut", "-board", "-spill", "-cut", "mill", "ford"};
    for (int a = 0; a < ARMS; a++) {
        double s = 0, r = 0, f = 0; int done = 0;
        for (int i = 0; i < ABLATION_SEEDS; i++) {
            RunResult x = run_job(ablation_job(a, i));
            s += x.seconds; r += x.red; f += x.fails; done += x.completed;
        }
        printf("%-7s seconds %7.2f red %6.2f lost %5.2f done %d/%d\n", name[a], s / ABLATION_SEEDS, r / ABLATION_SEEDS,
               f / ABLATION_SEEDS, done, ABLATION_SEEDS);
    }
    double crisis[L_LEGS] = {0}; int zero = 0, done = 0, n = TENSION_SEEDS * TENSION_RUNS;
    for (int i = 0; i < TENSION_SEEDS; i++)
        for (int k = 0; k < TENSION_RUNS; k++) {
            RunResult x = run_job(tension_job(i, k));
            int fails = 0;
            for (int l = 0; l < L_LEGS; l++) { crisis[l] += x.failure[l] > 0 ? x.failure[l] : x.crisis[l]; fails += x.failure[l]; }
            zero += fails == 0; done += x.completed;
        }
    printf("tension per leg:");
    for (int l = 0; l < L_LEGS; l++) printf(" %.2f", crisis[l] / n);
    printf("; no failure %d/%d; completed %d/%d\n", zero, n, done, n);
    return 0;
}

static int write_all(const char *dir)
{
    int np = PLAY_SEEDS, na = ARMS * ABLATION_SEEDS, nt = TENSION_SEEDS * TENSION_RUNS;
    RunResult *res = calloc((size_t)(np + na + nt), sizeof *res);
    if (!res) return 1;
    run_all(res, np, na);
    char row[1024];

    FILE *f = open_out(dir, "play-order.json");
    fprintf(f, "{\n  \"version\": 1,\n  \"generator\": \"%s\",\n  \"note\": \"Scenes open at fixed progress points: the start of the night and the end of a leg. "
               "stones 0 = the mill bell, 1 = the church bell; ford 0 = the ford bell, 1 = the hill siren. quiet lists the heavy scenes "
               "shown with no 'mark held' banner raised before the next frame of the line. Do not edit by hand.\",\n  \"seeds\": [\n", GEN);
    for (int i = 0; i < np; i++) { play_row(row, sizeof row, i, &res[i]); fprintf(f, "    %s%s\n", row, i + 1 < np ? "," : ""); }
    fprintf(f, "  ]\n}\n");
    fclose(f);

    const RunResult *ab = res + np;
    f = open_out(dir, "ablation.json");
    fprintf(f, "{\n  \"version\": 1,\n  \"generator\": \"%s\",\n  \"note\": \"Same %d seeds in every arm. on = all story numbers, the church bell and the hill siren; "
               "off = scenes still open but every story number is neutral; beats = only that scene's numbers removed; choices = one choice fixed per path, "
               "the other at its default. Do not edit by hand.\",\n", GEN, ABLATION_SEEDS);
    fprintf(f, "  \"metrics\": {\n"
               "    \"seconds\": {\"better\": \"lower\", \"kind\": \"outcome\", \"felt\": \"screen\", \"label\": \"t on the night clock at first light; every t is run through the solver (the ford bell's ring and a dry cell being cranked included, no flat additions)\"},\n"
               "    \"red\": {\"better\": \"lower\", \"kind\": \"outcome\", \"felt\": \"screen\", \"label\": \"t run with the cell gauge in the red (below 0.25); an empty cell stops the motor and the turn coasts or is lost\"}\n"
               "  },\n  \"effects\": {\n    \"on\": [\n");
    write_rows(f, "      ", ab, ARM_ON);
    fprintf(f, "    ],\n    \"off\": [\n");
    write_rows(f, "      ", ab, ARM_OFF);
    fprintf(f, "    ]\n  },\n  \"beats\": {\n");
    write_arm_pair(f, "hut", ab, ARM_HUT, 0);
    write_arm_pair(f, "bell", ab, ARM_BOARD, 0);
    write_arm_pair(f, "spill", ab, ARM_SPILL, 0);
    write_arm_pair(f, "cut", ab, ARM_CUT, 1);
    fprintf(f, "  },\n  \"choices\": {\n    \"stones\": {\"paths\": {\n      \"0\": [\n");
    write_rows(f, "        ", ab, ARM_MILL);
    fprintf(f, "      ],\n      \"1\": [\n");
    write_rows(f, "        ", ab, ARM_ON);
    fprintf(f, "      ]\n    }},\n    \"ford\": {\"paths\": {\n      \"0\": [\n");
    write_rows(f, "        ", ab, ARM_FORD);
    fprintf(f, "      ],\n      \"1\": [\n");
    write_rows(f, "        ", ab, ARM_ON);
    fprintf(f, "      ]\n    }}\n  }\n}\n");
    fclose(f);

    const RunResult *tn = res + np + na;
    f = open_out(dir, "tension.json");
    fprintf(f, "{\n  \"version\": 1,\n  \"generator\": \"%s\",\n  \"policy\": \"%s\",\n", GEN, POLICY);
    fprintf(f, "  \"crisis\": \"the cell gauge in the red (below 0.25) at any moment in the leg, or a failure (a failure counts as a crisis)\",\n"
               "  \"failure\": \"the cell ran dry (Kit cranks it, or after the cut the turn is lost), the turn ran off the posts or wandered "
               "70 t and was relaunched, or it struck the ford bell's arm\",\n"
               "  \"segments\": [\"to the weir\", \"to the bell\", \"back to the gorge\", \"the water in the hut\", \"up to the dam road\"],\n"
               "  \"note\": \"%d seeds x %d nights; every night has its own stirred seed; the path comes from the night number.\",\n  \"seeds\": [\n",
            TENSION_SEEDS, TENSION_RUNS);
    for (int i = 0; i < nt; i++) {
        tension_row(row, sizeof row, i / TENSION_RUNS, i % TENSION_RUNS, &tn[i]);
        fprintf(f, "    %s%s\n", row, i + 1 < nt ? "," : "");
    }
    fprintf(f, "  ]\n}\n");
    fclose(f);

    double sec = 0;
    for (int i = 0; i < ABLATION_SEEDS; i++) sec += ab[i].seconds;
    printf("story bench: %d nights; mean night %.1f t (default path)\n", np + na + nt, sec / ABLATION_SEEDS);
    free(res);
    return 0;
}

/* ------------------------------------------------------------------ check mode */
static char *slurp(const char *dir, const char *name)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "missing %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)size + 1);
    if (!buf || fread(buf, 1, (size_t)size, f) != (size_t)size) exit(1);
    buf[size] = 0;
    fclose(f);
    return buf;
}

static int cmp_key(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return x < y ? -1 : x > y;
}

static int check(const char *dir)
{
    int failures = 0;
    /* [seed] every keeper key in the bench is distinct (paired arms share a seed by design). */
    int nk = PLAY_SEEDS + ABLATION_SEEDS + TENSION_SEEDS * TENSION_RUNS, k = 0;
    uint64_t *keys = malloc(sizeof *keys * (size_t)nk);
    for (int i = 0; i < PLAY_SEEDS; i++) keys[k++] = play_key(i);
    for (int i = 0; i < ABLATION_SEEDS; i++) keys[k++] = ablation_key(i);
    for (int i = 0; i < TENSION_SEEDS; i++)
        for (int r = 0; r < TENSION_RUNS; r++) keys[k++] = tension_key(i, r);
    qsort(keys, (size_t)nk, sizeof *keys, cmp_key);
    for (int i = 1; i < nk; i++) if (keys[i] == keys[i - 1]) { fputs("[seed] duplicate run key\n", stderr); failures++; }
    free(keys);

    enum { CHECKS = 8 };
    Job jobs[CHECKS] = {play_job(0), play_job(PLAY_SEEDS - 1), ablation_job(ARM_ON, 0), ablation_job(ARM_OFF, 0),
                        ablation_job(ARM_MILL, ABLATION_SEEDS - 1), ablation_job(ARM_FORD, ABLATION_SEEDS - 1),
                        tension_job(0, 0), tension_job(TENSION_SEEDS - 1, TENSION_RUNS - 1)};
    char *play = slurp(dir, "play-order.json"), *abl = slurp(dir, "ablation.json"), *ten = slurp(dir, "tension.json");
    char row[1024];
    /* How many times each re-run row must appear: the default arm is reused by every beat and as one path of each choice. */
    static const int copies[CHECKS] = {1, 1, 7, 1, 1, 1, 1, 1};
    for (int i = 0; i < CHECKS; i++) {
        RunResult r = run_job(jobs[i]);
        const char *hay, *file;
        if (i < 2) { play_row(row, sizeof row, i ? PLAY_SEEDS - 1 : 0, &r); hay = play; file = "play-order"; }
        else if (i < 6) { ablation_row(row, sizeof row, i < 4 ? 0 : ABLATION_SEEDS - 1, &r); hay = abl; file = "ablation"; }
        else { tension_row(row, sizeof row, i == 6 ? 0 : TENSION_SEEDS - 1, i == 6 ? 0 : TENSION_RUNS - 1, &r); hay = ten; file = "tension"; }
        int found = 0;
        for (const char *c = hay; (c = strstr(c, row)); c++) found++;
        if (found != copies[i]) {
            fprintf(stderr, "[bench] %s.json does not match the code (%d of %d copies): %s\n", file, found, copies[i], row);
            failures++;
        }
    }
    free(play); free(abl); free(ten);
    if (failures) { fprintf(stderr, "story bench check: %d failure(s); run make story to regenerate\n", failures); return 1; }
    puts("story bench check: 8 re-run rows match docs/narrative; all run seeds distinct");
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 3 && strcmp(argv[1], "--check") == 0) return check(argv[2]);
    if (argc == 2 && strcmp(argv[1], "--summary") == 0) return summary();
    if (argc == 2) return write_all(argv[1]);
    fputs("usage: story_bench <dir> | story_bench --check <dir> | story_bench --summary\n", stderr);
    return 2;
}
