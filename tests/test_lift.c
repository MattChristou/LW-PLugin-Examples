#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "lift.h"

static LWDVector g_pos;

static void fake_get(LWItemParam p, LWTime t, LWDVector v)
{
    assert(p == LWIP_POSITION);
    v[0] = g_pos[0]; v[1] = g_pos[1]; v[2] = g_pos[2];
}
static void fake_set(LWItemParam p, const LWDVector v)
{
    assert(p == LWIP_POSITION);
    g_pos[0] = v[0]; g_pos[1] = v[1]; g_pos[2] = v[2];
}

int main(void)
{
    LWError            err = NULL;
    LWItemMotionAccess ma  = { 0 };
    LWInstance         inst = Lift_Create(NULL, NULL, &err);

    assert(inst && !err);
    ma.getParam = fake_get;
    ma.setParam = fake_set;

    g_pos[1] = 2.0;
    Lift_Evaluate(inst, &ma);
    assert(fabs(g_pos[1] - 3.0) < 1e-12);   /* default lift is 1 m */

    Lift_Destroy(inst);
    puts("test_lift: ok");
    return 0;
}
