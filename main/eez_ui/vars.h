#ifndef EEZ_LVGL_UI_VARS_H
#define EEZ_LVGL_UI_VARS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// enum declarations

// Flow global variables

enum FlowGlobalVariables {
    FLOW_GLOBAL_VARIABLE_SP_RAW = 0,
    FLOW_GLOBAL_VARIABLE_BOB_RAW = 1,
    FLOW_GLOBAL_VARIABLE_UST_RAW = 2,
    FLOW_GLOBAL_VARIABLE_PAR_CHANNEL = 3,
    FLOW_GLOBAL_VARIABLE_PAR_FMAX = 4,
    FLOW_GLOBAL_VARIABLE_PAR_FMIN = 5,
    FLOW_GLOBAL_VARIABLE_PAR_FBASE = 6,
    FLOW_GLOBAL_VARIABLE_PAR_ACCEL = 7,
    FLOW_GLOBAL_VARIABLE_PAR_DECEL = 8,
    FLOW_GLOBAL_VARIABLE_PAR_JOG = 9,
    FLOW_GLOBAL_VARIABLE_LEN_TXT = 10,
    FLOW_GLOBAL_VARIABLE_SPEED_TXT = 11,
    FLOW_GLOBAL_VARIABLE_PROD_TXT = 12,
    FLOW_GLOBAL_VARIABLE_OST_TXT = 13,
    FLOW_GLOBAL_VARIABLE_BATCHES_TXT = 14,
    FLOW_GLOBAL_VARIABLE_FREQ_TXT = 15,
    FLOW_GLOBAL_VARIABLE_STATE_TXT = 16,
    FLOW_GLOBAL_VARIABLE_CUR_TXT = 17,
    FLOW_GLOBAL_VARIABLE_VOLT_TXT = 18,
    FLOW_GLOBAL_VARIABLE_RPM_TXT = 19,
    FLOW_GLOBAL_VARIABLE_SP_TXT = 20,
    FLOW_GLOBAL_VARIABLE_LINK_TXT = 21,
    FLOW_GLOBAL_VARIABLE_P_MSG_TXT = 22,
    FLOW_GLOBAL_VARIABLE_FAULT_CODE_TXT = 23,
    FLOW_GLOBAL_VARIABLE_FAULT_NAME_TXT = 24,
    FLOW_GLOBAL_VARIABLE_FAULT_DESC_TXT = 25,
    FLOW_GLOBAL_VARIABLE_FAULT_TIME_TXT = 26,
    FLOW_GLOBAL_VARIABLE_JRN0_TXT = 27,
    FLOW_GLOBAL_VARIABLE_JRN1_TXT = 28,
    FLOW_GLOBAL_VARIABLE_JRN2_TXT = 29,
    FLOW_GLOBAL_VARIABLE_SNAP0_TXT = 30,
    FLOW_GLOBAL_VARIABLE_SNAP1_TXT = 31,
    FLOW_GLOBAL_VARIABLE_SNAP2_TXT = 32,
    FLOW_GLOBAL_VARIABLE_SNAP3_TXT = 33,
    FLOW_GLOBAL_VARIABLE_SNAP4_TXT = 34,
    FLOW_GLOBAL_VARIABLE_SNAP5_TXT = 35,
    FLOW_GLOBAL_VARIABLE_C_STATE_TXT = 36,
    FLOW_GLOBAL_VARIABLE_C_CFG1_TXT = 37,
    FLOW_GLOBAL_VARIABLE_C_CFG2_TXT = 38,
    FLOW_GLOBAL_VARIABLE_C_CNT1_TXT = 39,
    FLOW_GLOBAL_VARIABLE_C_CNT2_TXT = 40,
    FLOW_GLOBAL_VARIABLE_C_CMD1_TXT = 41,
    FLOW_GLOBAL_VARIABLE_C_CMD2_TXT = 42,
    FLOW_GLOBAL_VARIABLE_C_ERR_TXT = 43,
    FLOW_GLOBAL_VARIABLE_C_FRAME_TXT = 44
};

// Native global variables

extern int32_t get_var_sp_raw();
extern void set_var_sp_raw(int32_t value);
extern int32_t get_var_bob_raw();
extern void set_var_bob_raw(int32_t value);
extern int32_t get_var_ust_raw();
extern void set_var_ust_raw(int32_t value);
extern int32_t get_var_par_channel();
extern void set_var_par_channel(int32_t value);
extern int32_t get_var_par_fmax();
extern void set_var_par_fmax(int32_t value);
extern int32_t get_var_par_fmin();
extern void set_var_par_fmin(int32_t value);
extern int32_t get_var_par_fbase();
extern void set_var_par_fbase(int32_t value);
extern int32_t get_var_par_accel();
extern void set_var_par_accel(int32_t value);
extern int32_t get_var_par_decel();
extern void set_var_par_decel(int32_t value);
extern int32_t get_var_par_jog();
extern void set_var_par_jog(int32_t value);
extern const char *get_var_len_txt();
extern void set_var_len_txt(const char *value);
extern const char *get_var_speed_txt();
extern void set_var_speed_txt(const char *value);
extern const char *get_var_prod_txt();
extern void set_var_prod_txt(const char *value);
extern const char *get_var_ost_txt();
extern void set_var_ost_txt(const char *value);
extern const char *get_var_batches_txt();
extern void set_var_batches_txt(const char *value);
extern const char *get_var_freq_txt();
extern void set_var_freq_txt(const char *value);
extern const char *get_var_state_txt();
extern void set_var_state_txt(const char *value);
extern const char *get_var_cur_txt();
extern void set_var_cur_txt(const char *value);
extern const char *get_var_volt_txt();
extern void set_var_volt_txt(const char *value);
extern const char *get_var_rpm_txt();
extern void set_var_rpm_txt(const char *value);
extern const char *get_var_sp_txt();
extern void set_var_sp_txt(const char *value);
extern const char *get_var_link_txt();
extern void set_var_link_txt(const char *value);
extern const char *get_var_p_msg_txt();
extern void set_var_p_msg_txt(const char *value);
extern const char *get_var_fault_code_txt();
extern void set_var_fault_code_txt(const char *value);
extern const char *get_var_fault_name_txt();
extern void set_var_fault_name_txt(const char *value);
extern const char *get_var_fault_desc_txt();
extern void set_var_fault_desc_txt(const char *value);
extern const char *get_var_fault_time_txt();
extern void set_var_fault_time_txt(const char *value);
extern const char *get_var_jrn0_txt();
extern void set_var_jrn0_txt(const char *value);
extern const char *get_var_jrn1_txt();
extern void set_var_jrn1_txt(const char *value);
extern const char *get_var_jrn2_txt();
extern void set_var_jrn2_txt(const char *value);
extern const char *get_var_snap0_txt();
extern void set_var_snap0_txt(const char *value);
extern const char *get_var_snap1_txt();
extern void set_var_snap1_txt(const char *value);
extern const char *get_var_snap2_txt();
extern void set_var_snap2_txt(const char *value);
extern const char *get_var_snap3_txt();
extern void set_var_snap3_txt(const char *value);
extern const char *get_var_snap4_txt();
extern void set_var_snap4_txt(const char *value);
extern const char *get_var_snap5_txt();
extern void set_var_snap5_txt(const char *value);
extern const char *get_var_c_state_txt();
extern void set_var_c_state_txt(const char *value);
extern const char *get_var_c_cfg1_txt();
extern void set_var_c_cfg1_txt(const char *value);
extern const char *get_var_c_cfg2_txt();
extern void set_var_c_cfg2_txt(const char *value);
extern const char *get_var_c_cnt1_txt();
extern void set_var_c_cnt1_txt(const char *value);
extern const char *get_var_c_cnt2_txt();
extern void set_var_c_cnt2_txt(const char *value);
extern const char *get_var_c_cmd1_txt();
extern void set_var_c_cmd1_txt(const char *value);
extern const char *get_var_c_cmd2_txt();
extern void set_var_c_cmd2_txt(const char *value);
extern const char *get_var_c_err_txt();
extern void set_var_c_err_txt(const char *value);
extern const char *get_var_c_frame_txt();
extern void set_var_c_frame_txt(const char *value);

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_VARS_H*/