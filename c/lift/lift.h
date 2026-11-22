/* Lift: the callbacks the unit test drives directly (manual section 9). */
#ifndef LIFT_H
#define LIFT_H
#include <lwserver.h>
#include <lwhandler.h>
#include <lwmotion.h>

LWInstance Lift_Create(void *priv, void *context, LWError *err);
void       Lift_Destroy(LWInstance in);
void       Lift_Evaluate(LWInstance in, const LWItemMotionAccess *ma);
#endif
