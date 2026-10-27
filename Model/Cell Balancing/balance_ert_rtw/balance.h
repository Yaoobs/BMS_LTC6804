/*
 * File: balance.h
 *
 * Code generated for Simulink model 'balance'.
 *
 * Model version                  : 1.29
 * Simulink Coder version         : 25.2 (R2025b) 28-Jul-2025
 * C/C++ source code generated on : Tue Oct 27 15:25:21 2026
 *
 * Target selection: ert.tlc
 * Embedded hardware selection: ARM Compatible->ARM Cortex-M
 * Code generation objectives: Unspecified
 * Validation result: Not run
 */

#ifndef balance_h_
#define balance_h_
#ifndef balance_COMMON_INCLUDES_
#define balance_COMMON_INCLUDES_
#include "rtwtypes.h"
#endif                                 /* balance_COMMON_INCLUDES_ */

/* Macros for accessing real-time model data structure */
#ifndef rtmGetErrorStatus
#define rtmGetErrorStatus(rtm)         ((rtm)->errorStatus)
#endif

#ifndef rtmSetErrorStatus
#define rtmSetErrorStatus(rtm, val)    ((rtm)->errorStatus = (val))
#endif

/* Forward declaration for rtModel */
typedef struct tag_RTM_balance_T RT_MODEL_balance_T;

/* Real-time Model Data Structure */
struct tag_RTM_balance_T {
  const char_T * volatile errorStatus;
};

/* Imported (extern) block signals */
extern uint16_T cell_voltage[12];      /* '<Root>/cell_voltage' */

/*
 * Exported Global Signals
 *
 * Note: Exported global signals are block signals with an exported global
 * storage class designation.  Code generation will declare the memory for
 * these signals and export their symbols.
 *
 */
extern uint8_T cell_balance[12];       /* '<S1>/Data Type Conversion' */

/*
 * Exported Global Parameters
 *
 * Note: Exported global parameters are tunable parameters with an exported
 * global storage class designation.  Code generation will declare the memory for
 * these parameters and exports their symbols.
 *
 */
extern uint8_T Offset;                 /* Variable: Offset
                                        * Referenced by: '<S1>/Offset'
                                        */

/* Model entry point functions */
extern void balance_initialize(void);
extern void balance_step(void);

/* Real-time Model object */
extern RT_MODEL_balance_T *const balance_M;

/*-
 * These blocks were eliminated from the model due to optimizations:
 *
 * Block '<S1>/Reshape' : Reshape block reduction
 */

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
 * '<Root>' : 'balance'
 * '<S1>'   : 'balance/PassiveBalance'
 * '<S2>'   : 'balance/PassiveBalance/Mean'
 */
#endif                                 /* balance_h_ */

/*
 * File trailer for generated code.
 *
 * [EOF]
 */
