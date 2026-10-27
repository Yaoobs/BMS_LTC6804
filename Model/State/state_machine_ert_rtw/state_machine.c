/*
 * File: state_machine.c
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

#include "state_machine.h"
#include "rtwtypes.h"

/* Named constants for Chart: '<Root>/BMS_StateMachine' */
#define state_machine_IN_Charging      ((uint8_T)1U)
#define state_machine_IN_Driving       ((uint8_T)2U)
#define state_machine_IN_Fault         ((uint8_T)3U)
#define state_machine_IN_Standby       ((uint8_T)4U)

/* Exported block signals */
SRE StateRequest;                      /* '<Root>/StateRequest' */
boolean_T CriticalFault;               /* '<Root>/CriticalFault' */
BMS_State_Enum BMS_State;              /* '<Root>/BMS_StateMachine' */

/* Block states (default storage) */
DW_state_machine_T state_machine_DW;

/* Real-time model */
static RT_MODEL_state_machine_T state_machine_M_;
RT_MODEL_state_machine_T *const state_machine_M = &state_machine_M_;

/* Model step function */
void state_machine_step(void)
{
  /* Chart: '<Root>/BMS_StateMachine' incorporates:
   *  Inport: '<Root>/CriticalFault'
   *  Inport: '<Root>/StateRequest'
   */
  if (state_machine_DW.is_active_c3_state_machine == 0U) {
    state_machine_DW.is_active_c3_state_machine = 1U;
    state_machine_DW.is_c3_state_machine = state_machine_IN_Standby;
    BMS_State = BMS_Standby;
  } else {
    switch (state_machine_DW.is_c3_state_machine) {
     case state_machine_IN_Charging:
      if (CriticalFault) {
        state_machine_DW.is_c3_state_machine = state_machine_IN_Fault;
        BMS_State = BMS_Fault;
      } else if (StateRequest != Charging) {
        state_machine_DW.is_c3_state_machine = state_machine_IN_Standby;
        BMS_State = BMS_Standby;
      }
      break;

     case state_machine_IN_Driving:
      if (CriticalFault) {
        state_machine_DW.is_c3_state_machine = state_machine_IN_Fault;
        BMS_State = BMS_Fault;
      } else if (StateRequest != Driving) {
        state_machine_DW.is_c3_state_machine = state_machine_IN_Standby;
        BMS_State = BMS_Standby;
      }
      break;

     case state_machine_IN_Fault:
      if (!CriticalFault) {
        state_machine_DW.is_c3_state_machine = state_machine_IN_Standby;
        BMS_State = BMS_Standby;
      } else {
        BMS_State = BMS_Fault;
      }
      break;

     default:
      /* case IN_Standby: */
      if (StateRequest == Charging) {
        state_machine_DW.is_c3_state_machine = state_machine_IN_Charging;
        BMS_State = BMS_Charging;
      } else if (CriticalFault) {
        state_machine_DW.is_c3_state_machine = state_machine_IN_Fault;
        BMS_State = BMS_Fault;
      } else if (StateRequest == Driving) {
        state_machine_DW.is_c3_state_machine = state_machine_IN_Driving;
        BMS_State = BMS_Driving;
      }
      break;
    }
  }

  /* End of Chart: '<Root>/BMS_StateMachine' */
}

/* Model initialize function */
void state_machine_initialize(void)
{
  /* (no initialization code required) */
}

/*
 * File trailer for generated code.
 *
 * [EOF]
 */
