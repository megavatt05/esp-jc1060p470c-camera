#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Screens

enum ScreensEnum {
    _SCREEN_ID_FIRST = 1,
    SCREEN_ID_MAIN = 1,
    SCREEN_ID_PARAMS = 2,
    SCREEN_ID_ALARM = 3,
    SCREEN_ID_CONN = 4,
    _SCREEN_ID_LAST = 4
};

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *params;
    lv_obj_t *alarm;
    lv_obj_t *conn;
    lv_obj_t *top_main;
    lv_obj_t *lbl_title_main;
    lv_obj_t *link;
    lv_obj_t *lbl_link;
    lv_obj_t *meas;
    lv_obj_t *lbl_meas_cap;
    lv_obj_t *lbl_freq;
    lv_obj_t *lbl_freq_unit;
    lv_obj_t *mon;
    lv_obj_t *lbl_state_cap;
    lv_obj_t *lbl_state;
    lv_obj_t *lbl_cur_cap;
    lv_obj_t *lbl_cur;
    lv_obj_t *lbl_volt_cap;
    lv_obj_t *lbl_volt;
    lv_obj_t *lbl_rpm_cap;
    lv_obj_t *lbl_rpm;
    lv_obj_t *sp;
    lv_obj_t *lbl_sp_cap;
    lv_obj_t *sld_sp;
    lv_obj_t *btn_sp_dn;
    lv_obj_t *btn_sp_dn_lbl;
    lv_obj_t *lbl_sp_val;
    lv_obj_t *btn_sp_up;
    lv_obj_t *btn_sp_up_lbl;
    lv_obj_t *btn_start;
    lv_obj_t *btn_start_lbl;
    lv_obj_t *btn_stop;
    lv_obj_t *btn_stop_lbl;
    lv_obj_t *btn_jog;
    lv_obj_t *btn_jog_lbl;
    lv_obj_t *btn_rev;
    lv_obj_t *btn_rev_lbl;
    lv_obj_t *btn_fault_reset;
    lv_obj_t *btn_fault_reset_lbl;
    lv_obj_t *nav_main;
    lv_obj_t *btn_nav_main_main;
    lv_obj_t *btn_nav_main_main_lbl;
    lv_obj_t *btn_nav_params_main;
    lv_obj_t *btn_nav_params_main_lbl;
    lv_obj_t *btn_nav_alarm_main;
    lv_obj_t *btn_nav_alarm_main_lbl;
    lv_obj_t *btn_nav_conn_main;
    lv_obj_t *btn_nav_conn_main_lbl;
    lv_obj_t *top_params;
    lv_obj_t *lbl_title_params;
    lv_obj_t *lbl_top_right_params;
    lv_obj_t *btn_p_read;
    lv_obj_t *btn_p_read_lbl;
    lv_obj_t *btn_p_write;
    lv_obj_t *btn_p_write_lbl;
    lv_obj_t *lbl_p_msg;
    lv_obj_t *lbl_p0;
    lv_obj_t *spn_channel;
    lv_obj_t *lbl_p1;
    lv_obj_t *spn_fmax;
    lv_obj_t *lbl_p2;
    lv_obj_t *spn_fmin;
    lv_obj_t *lbl_p3;
    lv_obj_t *spn_fbase;
    lv_obj_t *lbl_p4;
    lv_obj_t *spn_accel;
    lv_obj_t *lbl_p5;
    lv_obj_t *spn_decel;
    lv_obj_t *lbl_p6;
    lv_obj_t *spn_jog;
    lv_obj_t *nav_params;
    lv_obj_t *btn_nav_main_params;
    lv_obj_t *btn_nav_main_params_lbl;
    lv_obj_t *btn_nav_params_params;
    lv_obj_t *btn_nav_params_params_lbl;
    lv_obj_t *btn_nav_alarm_params;
    lv_obj_t *btn_nav_alarm_params_lbl;
    lv_obj_t *btn_nav_conn_params;
    lv_obj_t *btn_nav_conn_params_lbl;
    lv_obj_t *top_alarm;
    lv_obj_t *lbl_title_alarm;
    lv_obj_t *cur;
    lv_obj_t *lbl_acode;
    lv_obj_t *lbl_aname;
    lv_obj_t *lbl_adesc;
    lv_obj_t *lbl_atime;
    lv_obj_t *btn_a_read;
    lv_obj_t *btn_a_read_lbl;
    lv_obj_t *btn_a_reset;
    lv_obj_t *btn_a_reset_lbl;
    lv_obj_t *jrn;
    lv_obj_t *lbl_jrn_cap;
    lv_obj_t *lbl_jrn0;
    lv_obj_t *lbl_jrn1;
    lv_obj_t *lbl_jrn2;
    lv_obj_t *lbl_snap_cap;
    lv_obj_t *lbl_snap0;
    lv_obj_t *lbl_snap1;
    lv_obj_t *lbl_snap2;
    lv_obj_t *lbl_snap3;
    lv_obj_t *lbl_snap4;
    lv_obj_t *lbl_snap5;
    lv_obj_t *nav_alarm;
    lv_obj_t *btn_nav_main_alarm;
    lv_obj_t *btn_nav_main_alarm_lbl;
    lv_obj_t *btn_nav_params_alarm;
    lv_obj_t *btn_nav_params_alarm_lbl;
    lv_obj_t *btn_nav_alarm_alarm;
    lv_obj_t *btn_nav_alarm_alarm_lbl;
    lv_obj_t *btn_nav_conn_alarm;
    lv_obj_t *btn_nav_conn_alarm_lbl;
    lv_obj_t *top_conn;
    lv_obj_t *lbl_title_conn;
    lv_obj_t *line;
    lv_obj_t *lbl_c_state;
    lv_obj_t *lbl_c_cfg_cap;
    lv_obj_t *lbl_c_cfg;
    lv_obj_t *lbl_c_cfg2;
    lv_obj_t *lbl_c_cnt_cap;
    lv_obj_t *lbl_c_cnt;
    lv_obj_t *lbl_c_cnt2;
    lv_obj_t *lbl_c_cmd_cap;
    lv_obj_t *lbl_c_cmd;
    lv_obj_t *lbl_c_cmd2;
    lv_obj_t *lbl_c_err_cap;
    lv_obj_t *lbl_c_err;
    lv_obj_t *lbl_c_frame;
    lv_obj_t *lbl_c_note;
    lv_obj_t *btn_c_reset;
    lv_obj_t *btn_c_reset_lbl;
    lv_obj_t *nav_conn;
    lv_obj_t *btn_nav_main_conn;
    lv_obj_t *btn_nav_main_conn_lbl;
    lv_obj_t *btn_nav_params_conn;
    lv_obj_t *btn_nav_params_conn_lbl;
    lv_obj_t *btn_nav_alarm_conn;
    lv_obj_t *btn_nav_alarm_conn_lbl;
    lv_obj_t *btn_nav_conn_conn;
    lv_obj_t *btn_nav_conn_conn_lbl;
} objects_t;

extern objects_t objects;

void create_screen_main();
void tick_screen_main();

void create_screen_params();
void tick_screen_params();

void create_screen_alarm();
void tick_screen_alarm();

void create_screen_conn();
void tick_screen_conn();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/