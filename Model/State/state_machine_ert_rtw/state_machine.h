/*
 * File: state_machine.h
 *
 * Code generated for Simulink model 'state_machine'.
 *
 * Model version                  : 1.19
 * Simulink Coder version         : 23.2 (R2023b) 01-Aug-2023
 * C/C++ source code generated on : Mon Oct 19 14:25:59 2026
 *
 * Target selection: ert.tlc
 * Embedded hardware selection: ARM Compatible->ARM Cortex-M
 * Code generation objectives: Unspecified
 * Validation result: Not run
 */

#ifndef RTW_HEADER_state_machine_h_
#define RTW_HEADER_state_machine_h_
#ifndef state_machine_COMMON_INCLUDES_
#define state_machine_COMMON_INCLUDES_
#include "rtwtypes.h"
#endif                                 /* state_machine_COMMON_INCLUDES_ */

/* Macros for accessing real-time model data structure */
#ifndef rtmGetErrorStatus
#define rtmGetErrorStatus(rtm)         ((rtm)->errorStatus)
#endif

#ifndef rtmSetErrorStatus
#define rtmSetErrorStatus(rtm, val)    ((rtm)->errorStatus = (val))
#endif

/* Forward declaration for rtModel */
typedef struct tag_RTM_state_machine_T RT_MODEL_state_machine_T;

#ifndef DEFINED_TYPEDEF_FOR_SRE_
#define DEFINED_TYPEDEF_FOR_SRE_

/* User State Request */
typedef uint8_T SRE;

/* enum SRE */
#define Standby                        ((SRE)0U)                 /* Default value */
#define Charging                       ((SRE)1U)
#define Driving                        ((SRE)2U)
#endif

#ifndef DEFINED_TYPEDEF_FOR_BMS_State_Enum_
#define DEFINED_TYPEDEF_FOR_BMS_State_Enum_

typedef uint8_T BMS_State_Enum;

/* enum BMS_State_Enum */
#define BMS_Standby                    ((BMS_State_Enum)0U)      /* Default value */
#define BMS_Charging                   ((BMS_State_Enum)1U)
#define BMS_Driving                    ((BMS_State_Enum)2U)
#define BMS_Fault                      ((BMS_State_Enum)3U)
#endif

/* Block states (default storage) for system '<Root>' */
typedef struct {
  uint8_T is_active_c3_state_machine;  /* '<Root>/BMS_StateMachine' */
  uint8_T is_c3_state_machine;         /* '<Root>/BMS_StateMachine' */
} DW_state_machine_T;

/* Real-time Model Data Structure */
struct tag_RTM_state_machine_T {
  const char_T * volatile errorStatus;
};

/* Block states (default storage) */
extern DW_state_machine_T state_machine_DW;

/*
 * Exported Global Signals
 *
 * Note: Exported global signals are block signals with an exported global
 * storage class designation.  Code generation will declare the memory for
 * these signals and export their symbols.
 *
 */
extern SRE StateRequest;               /* '<Root>/StateRequest' */
extern boolean_T CriticalFault;        /* '<Root>/CriticalFault' */
extern BMS_State_Enum BMS_State;       /* '<Root>/BMS_StateMachine' */

/* Model entry point functions */
extern void state_machine_initialize(void);
extern void state_machine_step(void);

/* Real-time Model object */
extern RT_MODEL_state_machine_T *const state_machine_M;

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
 * '<Root>' : 'state_machine'
 * '<S1>'   : 'state_machine/BMS_StateMachine'
 */
#endif                                 /* RTW_HEADER_state_machine_h_ */

/*
 * File trailer for generated code.
 *
 * [EOF]
 */
