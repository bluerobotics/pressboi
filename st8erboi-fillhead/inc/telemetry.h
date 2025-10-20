/**
 * @file telemetry.h
 * @brief Defines the telemetry field identifiers for the Fillhead controller.
 * @details AUTO-GENERATED FILE - DO NOT EDIT MANUALLY
 * Generated from telemetry.json on 2025-10-14
 * 
 * This header file defines all telemetry field keys used in the system.
 * To modify telemetry fields, edit telemetry.json and regenerate this file using generate_headers.py
 */
#pragma once

//==================================================================================================
// Telemetry Field Identifiers
//==================================================================================================

/**
 * @name Telemetry Field Keys
 * @brief String identifiers for telemetry data fields in the CSV-like format.
 * @details These defines specify the exact field names used in the telemetry string.
 * Format: "PRESSBOI_TELEM: field1:value1,field2:value2,..."
 * @{
 */

// --- Main System Telemetry ---
#define TELEM_KEY_MAIN_STATE              "MAIN_STATE"          ///< GUI: fillhead_main_state_var, Default: STANDBY

// --- Injector Telemetry ---
#define TELEM_KEY_INJ_T0                  "inj_t0"              ///< GUI: fillhead_torque0_var, Default: 0.0
#define TELEM_KEY_INJ_T1                  "inj_t1"              ///< GUI: fillhead_torque1_var, Default: 0.0
#define TELEM_KEY_INJ_H_MACH              "inj_h_mach"          ///< GUI: fillhead_homed0_var, Default: 0
#define TELEM_KEY_INJ_H_CART              "inj_h_cart"          ///< GUI: fillhead_homed1_var, Default: 0
#define TELEM_KEY_INJ_MACH_MM             "inj_mach_mm"         ///< GUI: fillhead_machine_steps_var, Default: 0.0
#define TELEM_KEY_INJ_CART_MM             "inj_cart_mm"         ///< GUI: fillhead_cartridge_steps_var, Default: 0.0
#define TELEM_KEY_INJ_CUMULATIVE_ML       "inj_cumulative_ml"   ///< GUI: fillhead_inject_cumulative_ml_var, Default: 0.0
#define TELEM_KEY_INJ_ACTIVE_ML           "inj_active_ml"       ///< GUI: fillhead_inject_active_ml_var, Default: 0.0
#define TELEM_KEY_INJ_TGT_ML              "inj_tgt_ml"          ///< GUI: fillhead_injection_target_ml_var, Default: 0.0
#define TELEM_KEY_ENABLED0                "enabled0"            ///< GUI: fillhead_enabled_state1_var, Default: 1
#define TELEM_KEY_ENABLED1                "enabled1"            ///< GUI: fillhead_enabled_state2_var, Default: 1
#define TELEM_KEY_INJ_ST                  "inj_st"              ///< GUI: fillhead_injector_state_var, Default: Standby

// --- Injection Valve Telemetry ---
#define TELEM_KEY_INJ_VALVE_POS           "inj_valve_pos"       ///< GUI: fillhead_inj_valve_pos_var, Default: 0.0
#define TELEM_KEY_INJ_VALVE_TORQUE        "inj_valve_torque"    ///< GUI: fillhead_torque2_var, Default: 0.0
#define TELEM_KEY_INJ_VALVE_HOMED         "inj_valve_homed"     ///< GUI: fillhead_inj_valve_homed_var, Default: 0
#define TELEM_KEY_INJ_V_ST                "inj_v_st"            ///< GUI: fillhead_inj_valve_state_var, Default: Not Homed

// --- Vacuum Valve Telemetry ---
#define TELEM_KEY_VAC_VALVE_POS           "vac_valve_pos"       ///< GUI: fillhead_vac_valve_pos_var, Default: 0.0
#define TELEM_KEY_VAC_VALVE_TORQUE        "vac_valve_torque"    ///< GUI: fillhead_vac_valve_torque_var, Default: 0.0
#define TELEM_KEY_VAC_VALVE_HOMED         "vac_valve_homed"     ///< GUI: fillhead_vac_valve_homed_var, Default: 0
#define TELEM_KEY_VAC_V_ST                "vac_v_st"            ///< GUI: fillhead_vac_valve_state_var, Default: Not Homed

// --- Heater Telemetry ---
#define TELEM_KEY_H_PV                    "h_pv"                ///< GUI: fillhead_temp_c_var, Default: 25.0
#define TELEM_KEY_H_SP                    "h_sp"                ///< GUI: fillhead_pid_setpoint_var, Default: 70.0
#define TELEM_KEY_H_ST_STR                "h_st_str"            ///< GUI: fillhead_heater_state_var, Default: Off

// --- Vacuum Telemetry ---
#define TELEM_KEY_VAC_PV                  "vac_pv"              ///< GUI: fillhead_vacuum_psig_var, Default: 0.5
#define TELEM_KEY_VAC_ST_STR              "vac_st_str"          ///< GUI: fillhead_vacuum_state_var, Default: Off

/** @} */

/**
 * @section Usage Example
 * @code
 * char buffer[256];
 * snprintf(buffer, sizeof(buffer), "%s%s:%s,%s:%.2f",
 *          TELEM_PREFIX,
 *          TELEM_KEY_MAIN_STATE, stateValue,
 *          TELEM_KEY_H_PV, temperatureValue);
 * @endcode
 */

