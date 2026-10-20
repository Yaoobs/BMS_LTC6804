/*
 * File: protect.c
 *
 * Code generated for Simulink model 'protect'.
 *
 * Model version                  : 1.38
 * Simulink Coder version         : 23.2 (R2023b) 01-Aug-2023
 * C/C++ source code generated on : Tue Oct 20 18:04:41 2026
 *
 * Target selection: ert.tlc
 * Embedded hardware selection: ARM Compatible->ARM Cortex-M
 * Code generation objectives: Unspecified
 * Validation result: Not run
 */

#include "protect.h"
#include "rtwtypes.h"

/* Named constants for Chart: '<Root>/voltage_limit_state' */
#define protect_IN_Normal              ((uint8_T)1U)
#define protect_IN_Overvoltage         ((uint8_T)2U)
#define protect_IN_Undervoltage        ((uint8_T)3U)

/* Exported block signals */
uint8_T UV_FLAG;                       /* '<Root>/voltage_limit_state' */
uint8_T OV_FLAG;                       /* '<Root>/voltage_limit_state' */

/* Block states (default storage) */
DW_protect_T protect_DW;

/* Real-time model */
static RT_MODEL_protect_T protect_M_;
RT_MODEL_protect_T *const protect_M = &protect_M_;

/* Model step function */
void protect_step(void)
{
  int32_T i;
  uint16_T rtb_Switch[12];
  uint16_T cell_voltage_0;
  uint16_T maxV;
  uint16_T u1;

  /* Switch: '<Root>/Switch' incorporates:
   *  Constant: '<Root>/Constant'
   *  Inport: '<Root>/cell_voltage'
   */
  for (i = 0; i < 12; i++) {
    cell_voltage_0 = cell_voltage[i];
    if (cell_voltage_0 > 0) {
      rtb_Switch[i] = cell_voltage_0;
    } else {
      rtb_Switch[i] = 9999U;
    }
  }

  /* End of Switch: '<Root>/Switch' */

  /* MinMax: '<Root>/min' */
  cell_voltage_0 = rtb_Switch[0];

  /* MinMax: '<Root>/max' incorporates:
   *  Inport: '<Root>/cell_voltage'
   */
  maxV = cell_voltage[0];
  for (i = 0; i < 11; i++) {
    /* MinMax: '<Root>/min' */
    u1 = rtb_Switch[i + 1];
    if (cell_voltage_0 > u1) {
      cell_voltage_0 = u1;
    }

    /* MinMax: '<Root>/max' incorporates:
     *  Inport: '<Root>/cell_voltage'
     */
    u1 = cell_voltage[i + 1];
    if (maxV < u1) {
      maxV = u1;
    }
  }

  /* Chart: '<Root>/voltage_limit_state' incorporates:
   *  MinMax: '<Root>/max'
   *  MinMax: '<Root>/min'
   */
  if (protect_DW.is_active_c3_protect == 0U) {
    protect_DW.is_active_c3_protect = 1U;
    protect_DW.is_c3_protect = protect_IN_Normal;
    UV_FLAG = 0U;
    OV_FLAG = 0U;
  } else {
    switch (protect_DW.is_c3_protect) {
     case protect_IN_Normal:
      UV_FLAG = 0U;
      OV_FLAG = 0U;
      if (cell_voltage_0 < 2800) {
        protect_DW.is_c3_protect = protect_IN_Undervoltage;
        UV_FLAG = 1U;
      } else if (maxV > 4200) {
        protect_DW.is_c3_protect = protect_IN_Overvoltage;
        OV_FLAG = 1U;
      }
      break;

     case protect_IN_Overvoltage:
      OV_FLAG = 1U;
      if (maxV < 4100) {
        protect_DW.is_c3_protect = protect_IN_Normal;
        UV_FLAG = 0U;
        OV_FLAG = 0U;
      }
      break;

     default:
      /* case IN_Undervoltage: */
      UV_FLAG = 1U;
      if (cell_voltage_0 > 3100) {
        protect_DW.is_c3_protect = protect_IN_Normal;
        UV_FLAG = 0U;
        OV_FLAG = 0U;
      }
      break;
    }
  }

  /* End of Chart: '<Root>/voltage_limit_state' */
}

/* Model initialize function */
void protect_initialize(void)
{
  /* (no initialization code required) */
}

/*
 * File trailer for generated code.
 *
 * [EOF]
 */
