#ifndef EEZ_LVGL_UI_EVENTS_H
#define EEZ_LVGL_UI_EVENTS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void action_nav_main(lv_event_t * e);
extern void action_nav_params(lv_event_t * e);
extern void action_nav_alarm(lv_event_t * e);
extern void action_nav_conn(lv_event_t * e);
extern void action_btn_start(lv_event_t * e);
extern void action_btn_stop(lv_event_t * e);
extern void action_btn_jog(lv_event_t * e);
extern void action_btn_rev(lv_event_t * e);
extern void action_btn_fault_reset(lv_event_t * e);
extern void action_sp_up(lv_event_t * e);
extern void action_sp_dn(lv_event_t * e);
extern void action_ust_d100(lv_event_t * e);
extern void action_ust_d10(lv_event_t * e);
extern void action_ust_d1(lv_event_t * e);
extern void action_ust_u1(lv_event_t * e);
extern void action_ust_u10(lv_event_t * e);
extern void action_ust_u100(lv_event_t * e);
extern void action_par_read(lv_event_t * e);
extern void action_par_write(lv_event_t * e);
extern void action_journal_read(lv_event_t * e);
extern void action_fault_reset(lv_event_t * e);
extern void action_counters_reset(lv_event_t * e);

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_EVENTS_H*/