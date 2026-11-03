#include "line.h"

#include <math.h>
#include <string.h>

#define PI 3.14159265358979323846
#define L_N 1200               /* grid: x in [-60, 60), dx 0.1, dt 0.05 (the same scheme as the observatory) */
#define L_X0 (-60.0)
#define L_DX 0.1
#define L_DT 0.05
#define L_SUBSTEPS 4           /* one frame = four solver steps = 0.2 t */
#define MOTOR 0.08             /* |eps| of the winding motor: a torque 2 pi eps = 0.50 on the turn */
#define RAMP LINE_RAMP           /* a sudden torque rings the whole line; spinning up over one period of its slowest mode does not */
#define CELL 0.30              /* charge at the start */
#define DRAIN 0.005            /* charge per t while the motor runs */
#define RED_LINE 0.30          /* the cell gauge turns red below this (a crisis in the bench) */
#define CRANK_CHARGE 0.15      /* Kit cranks a dry cell from her end, this much */
#define CRANK_WAIT 3.0         /* t the motor stays dead while she cranks */
#define RELAUNCH_CHARGE 0.30   /* a lost turn: Ada winds a fresh one at the last mark and tops the cell up to this */
#define PARK_DX 1.0            /* the catch arm reaches this far from the mark */
#define PARK_V 0.08            /* and takes only a turn slower than this */
#define PARK_T 0.6             /* standing on the mark, motor off, this long */
#define LEG_LIMIT 200.0         /* t without a held mark: the turn is lost and relaunched */
#define RUNOFF 52.0            /* past this the turn runs off the end of the posts: lost */
#define BASE_WOBBLE 0.10       /* absolute wobble of the speed readout with nobody reading the far end */

#define HUT_CELL 0.45          /* LE_HUT: Ada winds the cell past the line */
#define CALL_WOBBLE 0.10       /* LE_HUT: Kit's call steadies the readout (wobble x0.10) */
#define MILL_X 30.0            /* LE_ROUTE: the mill bell */
#define CHURCH_X 42.0          /* LE_ROUTE: the church bell */
#define CHURCH_HOLD 8.0        /* LE_ROUTE: t the turn must stand on the church mark while the sexton cranks */
#define SEXTON_CHARGE 0.20     /* LE_ROUTE: what the sexton cranks back up the line while his bell rings */
#define BOARD_CHARGE 0.28      /* LE_BOARD: Kit's board cell */
#define SPILL_WOBBLE 0.03      /* LE_SPILL: extra readout wobble in leg 4 */
#define SPILL_LEAK 0.006       /* LE_SPILL: charge the wet cell bleeds per t in leg 4, running or not */
#define FORD_X 44.0            /* LE_FORD: the ford bell, past the water line */
#define FORD_CHARGE 0.40       /* LE_FORD: the ferry house's cell, fed into the line while the bell rings */
#define FORD_HOLD 10.0         /* LE_FORD: t the turn must stay on the ford bell while it rings */
#define FORD_FAST 0.15         /* LE_FORD: a turn faster than this strikes the bell's arm and jams the motor */
#define KIT_FAST 0.30          /* a turn faster than this strikes Kit's catch arms (she takes the jolt) */
#define SLACK 0.70            /* LE_CUT: the slack line slips on the drum: torque x0.70 */
#define DAM_FAST 0.12          /* LE_CUT: Ada's old arm at the dam road jams under anything faster than this */

/* Leg marks: 0 -> 18 (the weir) -> the bell (mill 30 or church 42; 36 with no route effect) -> 20 (the gorge)
 * -> 32 (the hill siren, or the ford bell at 46) -> -10 (the dam road, back up the line to Ada). */
static const double TARGET[L_LEGS] = {18.0, 36.0, 20.0, 32.0, -10.0};

/* The spill, the ford and the cut get the screen to themselves: no "mark held" banner before or after. */
static const int HEAVY[LS_COUNT] = {0, 0, 0, 1, 1, 1, 0};

int line_options(int scene) { return scene == LS_STONES || scene == LS_FORD ? 2 : 1; }
int line_heavy(int scene) { return scene >= 0 && scene < LS_COUNT && HEAVY[scene]; }
int line_ending(const Line *l) { return l->scene == LS_DAWN || l->leg >= L_LEGS ? l->ford : -1; }

static int on(const Line *l, unsigned effect) { return (l->effects & effect) != 0; }

double line_torque(const Line *l)
{
    /* With the lower posts gone the slack line slips on the winding drum (LE_CUT). */
    return l->leg >= 4 && !l->count && on(l, LE_CUT) ? MOTOR * SLACK : MOTOR;
}

double line_stop_distance(const Line *l, double v)
{
    double s = fmin(fabs(v), 0.95);
    return 8.0 * (1.0 / sqrt(1.0 - s * s) - 1.0) / (2.0 * PI * line_torque(l));
}

double line_brake_speed(const Line *l, double d)
{
    if (d <= 0.0) return 0.0;
    double g = 1.0 + d * 2.0 * PI * line_torque(l) / 8.0;
    return sqrt(1.0 - 1.0 / (g * g));
}

double line_motor(const Line *l) { return l->ready ? -l->field.eps / line_torque(l) : 0.0; }

double line_coast_speed(const Line *l, double v)
{
    /* Spinning down from torque e takes |e| RAMP / MOTOR t at a falling torque: the turn gains about half of what that
     * torque would add over that time. */
    double e = l->ready ? -l->field.eps : 0.0;
    return v + 2.0 * PI * e / 8.0 * fabs(e) * RAMP / MOTOR / 2.0;
}

double line_wobble(const Line *l)
{
    if (l->leg >= L_LEGS) return 0.0;
    double amp = BASE_WOBBLE;
    if (l->count && on(l, LE_HUT)) amp *= CALL_WOBBLE;
    if (l->leg == 3 && on(l, LE_SPILL)) amp += SPILL_WOBBLE;
    /* The same smooth wobble for everyone who reads the gauge: it depends only on the night clock. */
    double t = l->time;
    return amp * (0.6 * sin(1.7 * t + 0.3) + 0.4 * sin(4.3 * t + 2.1));
}

double line_leak(const Line *l)
{
    /* The relay hut is taking water in leg 4; held on the ford bell, the ferry house's dry cell carries the motor. */
    if (l->leg != 3 || !on(l, LE_SPILL) || l->held > 0.0) return 0.0;
    return SPILL_LEAK;
}

double line_arm_limit(const Line *l)
{
    if (l->leg == 3 && line_hold_need(l) > 0.0) return FORD_FAST;
    return l->count || !on(l, LE_CUT) ? KIT_FAST : DAM_FAST;
}

double line_hold_need(const Line *l)
{
    if (l->leg == 1 && l->route == 1 && on(l, LE_ROUTE)) return CHURCH_HOLD;
    return l->leg == 3 && l->ford == 0 && on(l, LE_FORD) ? FORD_HOLD : 0.0;
}

/* Charge that comes up the line once a held mark starts ringing: the sexton's crank, or the ferry house's cell. */
static double hold_charge(const Line *l) { return l->leg == 1 ? SEXTON_CHARGE : FORD_CHARGE; }

int line_cue(const Line *l)
{
    /* Kit reads the true turn at her end: "brake" once the stopping distance reaches the mark, "let it go" once it
     * stands on the mark. Nothing once her end of the line is gone. */
    if (!l->count || !on(l, LE_HUT) || l->scene != LS_NONE || l->leg >= L_LEGS) return 0;
    double rem = l->target_x - l->x, cv = line_coast_speed(l, l->v), toward = rem >= 0.0 ? cv : -cv;
    if (fabs(rem) < PARK_DX && fabs(cv) < PARK_V) return 2;
    if (toward > 0.02 && toward > 0.9 * line_brake_speed(l, fabs(rem) - toward * RAMP * 0.8 - 0.3 * PARK_DX)) return 1;
    return 0;
}

static void open_scene(Line *l, int scene)
{
    l->scene = scene;
    if (l->order_len < LS_COUNT) l->order[l->order_len++] = scene;
    if (line_heavy(scene)) { l->banner = LB_NONE; l->banner_left = 0.0; }
}

static void raise_banner(Line *l, int banner)
{
    l->banner = banner;
    l->banner_left = 1.5;
    if (banner == LB_HELD) l->loud++;
}

static void start_leg(Line *l)
{
    l->from_x = l->x;
    l->target_x = TARGET[l->leg];
    if (l->leg == 1 && l->route >= 0 && on(l, LE_ROUTE)) l->target_x = l->route == 0 ? MILL_X : CHURCH_X;
    if (l->leg == 3 && l->ford == 0 && on(l, LE_FORD)) l->target_x = FORD_X;
    l->leg_time = l->parked = l->held = 0.0;
    l->knocked = 0;
}

void line_free(Line *l)
{
    if (l->ready) field_free(&l->field);
    l->ready = 0;
}

/* Allocates the field; a Line that was initialised before must be released with line_free first. */
void line_init(Line *l, unsigned effects)
{
    memset(l, 0, sizeof *l);
    l->ready = field_init(&l->field, L_N, L_X0, L_DX, L_DT, 0) == 0;
    if (l->ready) field_set_kink(&l->field, 0.0, 0.0, 1);
    l->effects = effects;
    l->route = l->ford = -1;
    l->charge = CELL;
    l->scene = LS_NONE;
    open_scene(l, LS_HUT);
}

static void clear_leg(Line *l)
{
    static const int next[L_LEGS] = {LS_STONES, LS_BELL, LS_FORD, LS_CUT, LS_DAWN};
    int scene = next[l->leg];
    /* The church's sexton winds his crank once his bell has rung, and sends the charge back up the line. */
    l->leg++;
    /* The heavy scenes and the ending get the screen to themselves: no "mark held" banner. */
    if (!line_heavy(scene) && scene != LS_DAWN) raise_banner(l, LB_HELD);
    open_scene(l, scene);
}

static void fail(Line *l, int banner)
{
    l->fails++;
    l->fails_leg[l->leg]++;
    l->crisis_leg[l->leg] = 1;
    raise_banner(l, banner);
}

/* A lost turn (it ran off the posts, or wandered too long, or the cell went dry with nobody at Kit's end to crank):
 * Ada winds a fresh turn at the last mark, at rest, and tops the cell up. The background ripples go with the old one. */
static void relaunch(Line *l)
{
    l->field.eps = 0.0;
    field_set_kink(&l->field, l->from_x, 0.0, 1);
    l->x = l->from_x;
    l->v = 0.0;
    l->leg_time = l->parked = l->held = 0.0;
    l->drift = 0.0;
    l->refuel = 0;
    if (l->charge < RELAUNCH_CHARGE) l->charge = RELAUNCH_CHARGE;
    fail(l, LB_LOST);
}

void line_tick(Line *l, int frames, int command)
{
    if (!l->ready || frames < 0 || l->scene != LS_NONE || l->leg >= L_LEGS) return;
    if (frames > 250) frames = 250;
    if (command < LC_COAST || command > LC_UP) command = LC_COAST;
    for (int f = 0; f < frames && l->scene == LS_NONE && l->leg < L_LEGS; f++) {
        int leg = l->leg;
        double before = l->x;
        l->command = command;
        for (int k = 0; k < L_SUBSTEPS; k++) {
            int run = 0;
            if (l->drift > 0.0 && (l->drift -= L_DT) <= 1e-12) {
                l->drift = 0.0;
                if (l->refuel && l->charge < CRANK_CHARGE) l->charge = CRANK_CHARGE;
                l->refuel = 0;
            }
            if (l->charge > 0.0 && l->drift <= 0.0 && command != LC_COAST) run = command == LC_DOWN ? 1 : -1;
            /* Positive bias pushes a rising kink to -x (force -2 pi eps), so running the motor down the valley is eps < 0.
             * The motor spins up and down over RAMP t; it draws charge in proportion to how hard it turns. */
            double want = -line_torque(l) * run, step = MOTOR * L_DT / RAMP;
            l->field.eps += fmax(-step, fmin(step, want - l->field.eps));
            field_step(&l->field);
            l->charge = fmax(0.0, l->charge - DRAIN * L_DT * fabs(l->field.eps) / line_torque(l) - line_leak(l) * L_DT);
            l->time += L_DT;
            l->leg_time += L_DT;
            if (l->charge < RED_LINE) { l->red += L_DT; l->crisis_leg[leg] = 1; }
        }
        double frame = L_DT * L_SUBSTEPS, x = field_kink_position(&l->field);
        l->frames++;
        /* A reading that jumps more than a turn can run in a frame means the line shook loose (ripples crossed pi
         * somewhere): treat it as a lost turn. */
        int shook = !isfinite(x) || fabs(x - before) > 2.0;
        if (shook) x = l->x;
        l->x = x;
        l->v = (x - before) / frame;
        if (l->banner_left > 0.0 && (l->banner_left -= frame) <= 0.0) l->banner = LB_NONE;
        double need = line_hold_need(l);
        int near = fabs(l->x - l->target_x) < PARK_DX, still = fabs(l->v) < PARK_V;
        if (l->held > 0.0) {
            /* On a held mark (the church while the sexton cranks, the ford bell while it rings): the leg ends once it
             * has rung long enough. The clock runs while you wait. */
            if (near && (l->held += frame) >= need - 1e-9) { clear_leg(l); continue; }
            if (!near) l->held = 0.0;   /* the turn slid off the arm: start the ring again */
        } else if (near && still && command == LC_COAST && fabs(l->field.eps) < 0.1 * MOTOR && l->drift <= 0.0) {
            if ((l->parked += frame) >= PARK_T - 1e-9) {
                if (need <= 0.0) { clear_leg(l); continue; }
                l->charge += hold_charge(l);
                l->held = frame;
            }
        } else {
            l->parked = 0.0;
        }
        /* A turn that hits a catch arm too fast jams the motor (once per leg). Kit can take a hard one at her arms;
         * the ford bell's arm sits low in the water, and Ada's old arm at the dam road is stiff. */
        if (near && fabs(l->v) > line_arm_limit(l) && !l->knocked && l->held <= 0.0) {
            l->knocked = 1;
            l->drift = CRANK_WAIT;
            fail(l, LB_KNOCK);
        }
        if (shook || fabs(l->x) > RUNOFF || l->leg_time > LEG_LIMIT) relaunch(l);
        /* While Kit's end is alive she cranks a dry cell where it is (the turn coasts meanwhile). After the cut nobody
         * can, and the turn is lost (LE_CUT). */
        else if (l->charge <= 0.0 && l->drift <= 0.0) {
            if (l->count || !on(l, LE_CUT)) { l->drift = CRANK_WAIT; l->refuel = 1; fail(l, LB_DRY); }
            else relaunch(l);
        }
    }
}

int line_answer(Line *l, int option)
{
    int scene = l->scene;
    if (scene == LS_NONE || option < 0 || option >= line_options(scene)) return 1;
    l->scene = LS_NONE;
    switch (scene) {
    case LS_HUT:
        l->count = 1;
        if (on(l, LE_HUT)) l->charge = HUT_CELL;
        start_leg(l);
        break;
    case LS_STONES:
        l->route = option;
        start_leg(l);
        break;
    case LS_BELL:
        if (on(l, LE_BOARD)) l->charge += BOARD_CHARGE;
        open_scene(l, LS_SPILL);
        break;
    case LS_SPILL:
        start_leg(l);
        break;
    case LS_FORD:
        l->ford = option;
        start_leg(l);
        break;
    case LS_CUT:
        if (on(l, LE_CUT)) l->count = 0;
        start_leg(l);
        break;
    case LS_DAWN:
        break;
    default:
        return 1;
    }
    return 0;
}

double line_value(const Line *l, int field)
{
    switch (field) {
    case LV_X: return l->x;
    case LV_V: return l->v;
    case LV_V_READ: return l->v + line_wobble(l);
    case LV_TIME: return l->time;
    case LV_CHARGE: return l->charge;
    case LV_RED: return l->red;
    case LV_LEG: return l->leg;
    case LV_SCENE: return l->scene;
    case LV_BANNER: return l->banner;
    case LV_FAILS: return l->fails;
    case LV_FROM: return l->from_x;
    case LV_TARGET: return l->target_x;
    case LV_REMAINING: return l->target_x - l->x;
    case LV_ROUTE: return l->route;
    case LV_FORD: return l->ford;
    case LV_COUNT_ON: return l->count && on(l, LE_HUT);
    case LV_WOBBLE: return line_wobble(l);
    case LV_ENDING: return line_ending(l);
    case LV_LOUD: return l->loud;
    case LV_HEAVY: return line_heavy(l->scene);
    case LV_COMMAND: return l->command;
    case LV_RED_LINE: return RED_LINE;
    case LV_HELD: return l->held;
    case LV_HOLD_NEED: return line_hold_need(l);
    case LV_LEAK: return line_leak(l);
    case LV_DRIFT: return l->drift;
    case LV_CUE: return line_cue(l);
    case LV_PARKED: return l->parked;
    case LV_ENERGY: return l->ready ? field_energy(&l->field) : 0.0;
    case LV_WIDTH: return l->ready ? 2.0 / field_max_gradient(&l->field) : 0.0;
    case LV_STOP_DISTANCE: return line_stop_distance(l, l->v);
    case LV_MOTOR: return line_motor(l);
    case LV_COAST_V: return line_coast_speed(l, l->v + line_wobble(l));
    default: return 0.0;
    }
}
