/*
 * File: protect.h
 *
 * Code generated for Simulink model 'protect'.
 *
 * Model version                  : 1.40
 * Simulink Coder version         : 23.2 (R2023b) 01-Aug-2023
 * C/C++ source code generated on : Fri Oct 23 11:59:35 2026
 *
 * Target selection: ert.tlc
 * Embedded hardware selection: ARM Compatible->ARM Cortex-M
 * Code generation objectives: Unspecified
 * Validation result: Not run
 */

#ifndef RTW_HEADER_protect_h_
#define RTW_HEADER_protect_h_
#ifndef protect_COMMON_INCLUDES_
#define protect_COMMON_INCLUDES_
#include "rtwtypes.h"
#endif                                 /* protect_COMMON_INCLUDES_ */

/* Macros for accessing real-time model data structure */
#ifndef rtmGetErrorStatus
#define rtmGetErrorStatus(rtm)         ((rtm)->errorStatus)
#endif

#ifndef rtmSetErrorStatus
#define rtmSetErrorStatus(rtm, val)    ((rtm)->errorStatus = (val))
#endif

/* Forward declaration for rtModel */
typedef struct tag_RTM_protect_T RT_MODEL_protect_T;

/* Block states (default storage) for system '<Root>' */
typedef struct {
  uint8_T is_active_c3_protect;        /* '<Root>/voltage_limit_state' */
  uint8_T is_c3_protect;               /* '<Root>/voltage_limit_state' */
} DW_protect_T;

/* Real-time Model Data Structure */
struct tag_RTM_protect_T {
  const char_T * volatile errorStatus;
};

/* Imported (extern) block signals */
extern uint16_T cell_voltage[12];      /* '<Root>/cell_voltage' */

/* Block states (default storage) */
extern DW_protect_T protect_DW;

/*
 * Exported Global Signals
 *
 * Note: Exported global signals are block signals with an exported global
 * storage class designation.  Code generation will declare the memory for
 * these signals and export their symbols.
 *
 */
extern uint8_T UV_FLAG;                /* '<Root>/voltage_limit_state' */
extern uint8_T OV_FLAG;                /* '<Root>/voltage_limit_state' */

/* Model entry point functions */
extern void protect_initialize(void);
extern void protect_step(void);

/* Real-time Model object */
extern RT_MODEL_protect_T *const protect_M;

/*-
 * The generated code includes comments that allow you to trace directly
 * back to the appropriate location in the model.  The basic format
 * is <system>/block_name, where system is the system number (uniquely
 * assigned by Simulink) and block_name is the name of the block.
 *
 * Use the MATLAB hilite_system command to trace the generated code back
 * to the model.  For example,
 *
 * hilite_system('<S3>')    - opens system 3
 * hilite_system('<S3>/Kp') - opens and selects block Kp which resides in S3
 *
 * Here is the system hierarchy for this model
 *
 * '<Root>' : 'protect'
 * '<S1>'   : 'protect/voltage_limit_state'
 */
#endif                                 /* RTW_HEADER_protect_h_ */

/*
 * File trailer for generated code.
 *
 * [EOF]
 */
