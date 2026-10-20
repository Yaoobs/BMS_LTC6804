/*
 * File: Ah_Integrator_Estimator.h
 *
 * Code generated for Simulink model 'Ah_Integrator_Estimator'.
 *
 * Model version                  : 1.49
 * Simulink Coder version         : 23.2 (R2023b) 01-Aug-2023
 * C/C++ source code generated on : Tue Oct 20 10:15:13 2026
 *
 * Target selection: ert.tlc
 * Embedded hardware selection: ARM Compatible->ARM Cortex-M
 * Code generation objectives: Unspecified
 * Validation result: Not run
 */

#ifndef RTW_HEADER_Ah_Integrator_Estimator_h_
#define RTW_HEADER_Ah_Integrator_Estimator_h_
#ifndef Ah_Integrator_Estimator_COMMON_INCLUDES_
#define Ah_Integrator_Estimator_COMMON_INCLUDES_
#include "rtwtypes.h"
#endif                            /* Ah_Integrator_Estimator_COMMON_INCLUDES_ */

/* Macros for accessing real-time model data structure */
#ifndef rtmGetErrorStatus
#define rtmGetErrorStatus(rtm)         ((rtm)->errorStatus)
#endif

#ifndef rtmSetErrorStatus
#define rtmSetErrorStatus(rtm, val)    ((rtm)->errorStatus = (val))
#endif

/* Forward declaration for rtModel */
typedef struct tag_RTM_Ah_Integrator_Estimat_T RT_MODEL_Ah_Integrator_Estima_T;

/* Block states (default storage) for system '<Root>' */
typedef struct {
  real32_T DiscreteTimeIntegrator_DSTATE;/* '<S1>/Discrete-Time Integrator' */
} DW_Ah_Integrator_Estimator_T;

/* Real-time Model Data Structure */
struct tag_RTM_Ah_Integrator_Estimat_T {
  const char_T * volatile errorStatus;
};

/* Imported (extern) block signals */
extern uint16_T InitialSOC;            /* '<Root>/InitialSOC' */
extern int16_T Current;                /* '<Root>/Current' */

/* Block states (default storage) */
extern DW_Ah_Integrator_Estimator_T Ah_Integrator_Estimator_DW;

/*
 * Exported Global Signals
 *
 * Note: Exported global signals are block signals with an exported global
 * storage class designation.  Code generation will declare the memory for
 * these signals and export their symbols.
 *
 */
extern uint16_T SOC;                   /* '<S1>/single->uint8' */

/*
 * Exported Global Parameters
 *
 * Note: Exported global parameters are tunable parameters with an exported
 * global storage class designation.  Code generation will declare the memory for
 * these parameters and exports their symbols.
 *
 */
extern uint16_T Q_rated;               /* Variable: Q_rated
                                        * Referenced by: '<S1>/总容量参数-mA'
                                        */
extern uint8_T eta_coulomb;            /* Variable: eta_coulomb
                                        * Referenced by: '<S1>/库伦效率系数'
                                        */

/* Model entry point functions */
extern void Ah_Integrator_Estimator_initialize(void);
extern void Ah_Integrator_Estimator_step(void);

/* Real-time Model object */
extern RT_MODEL_Ah_Integrator_Estima_T *const Ah_Integrator_Estimator_M;

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
 * '<Root>' : 'Ah_Integrator_Estimator'
 * '<S1>'   : 'Ah_Integrator_Estimator/Ah_Integrator_Estimator'
 */
#endif                               /* RTW_HEADER_Ah_Integrator_Estimator_h_ */

/*
 * File trailer for generated code.
 *
 * [EOF]
 */
