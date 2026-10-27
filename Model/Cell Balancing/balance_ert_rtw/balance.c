/*
 * File: balance.c
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

#include "balance.h"
#include "rtwtypes.h"
#ifndef UCHAR_MAX
#include <limits.h>
#endif

#if ( UCHAR_MAX != (0xFFU) ) || ( SCHAR_MAX != (0x7F) )
#error Code was generated for compiler with different sized uchar/char. \
Consider adjusting Test hardware word size settings on the \
Hardware Implementation pane to match your compiler word sizes as \
defined in limits.h of the compiler. Alternatively, you can \
select the Test hardware is the same as production hardware option and \
select the Enable portable word sizes option on the Code Generation > \
Verification pane for ERT based targets, which will disable the \
preprocessor word size checks.
#endif

#if ( USHRT_MAX != (0xFFFFU) ) || ( SHRT_MAX != (0x7FFF) )
#error Code was generated for compiler with different sized ushort/short. \
Consider adjusting Test hardware word size settings on the \
Hardware Implementation pane to match your compiler word sizes as \
defined in limits.h of the compiler. Alternatively, you can \
select the Test hardware is the same as production hardware option and \
select the Enable portable word sizes option on the Code Generation > \
Verification pane for ERT based targets, which will disable the \
preprocessor word size checks.
#endif

#if ( UINT_MAX != (0xFFFFFFFFU) ) || ( INT_MAX != (0x7FFFFFFF) )
#error Code was generated for compiler with different sized uint/int. \
Consider adjusting Test hardware word size settings on the \
Hardware Implementation pane to match your compiler word sizes as \
defined in limits.h of the compiler. Alternatively, you can \
select the Test hardware is the same as production hardware option and \
select the Enable portable word sizes option on the Code Generation > \
Verification pane for ERT based targets, which will disable the \
preprocessor word size checks.
#endif

#if ( ULONG_MAX != (0xFFFFFFFFU) ) || ( LONG_MAX != (0x7FFFFFFF) )
#error Code was generated for compiler with different sized ulong/long. \
Consider adjusting Test hardware word size settings on the \
Hardware Implementation pane to match your compiler word sizes as \
defined in limits.h of the compiler. Alternatively, you can \
select the Test hardware is the same as production hardware option and \
select the Enable portable word sizes option on the Code Generation > \
Verification pane for ERT based targets, which will disable the \
preprocessor word size checks.
#endif

/* Exported block signals */
uint8_T cell_balance[12];              /* '<S1>/Data Type Conversion' */

/* Exported block parameters */
uint8_T Offset = 16U;                  /* Variable: Offset
                                        * Referenced by: '<S1>/Offset'
                                        */

/* Real-time model */
static RT_MODEL_balance_T balance_M_;
RT_MODEL_balance_T *const balance_M = &balance_M_;

/* Model step function */
void balance_step(void)
{
  int32_T k;
  real32_T sum_val;
  uint16_T cell_voltage_0;
  uint16_T count;
  uint16_T maxV;

  /* MinMax: '<S1>/Max' incorporates:
   *  Inport: '<Root>/cell_voltage'
   */
  maxV = cell_voltage[0];
  for (k = 0; k < 11; k++) {
    count = cell_voltage[k + 1];
    if (maxV < count) {
      maxV = count;
    }
  }

  /* MATLAB Function: '<S1>/Mean' incorporates:
   *  Inport: '<Root>/cell_voltage'
   */
  sum_val = 0.0F;
  count = 0U;
  for (k = 0; k < 12; k++) {
    cell_voltage_0 = cell_voltage[k];
    if (cell_voltage_0 > 0) {
      sum_val += (real32_T)cell_voltage_0;
      count++;
    }
  }

  if (count > 0) {
    sum_val /= (real32_T)count;
  } else {
    sum_val = 0.0F;
  }

  /* End of MATLAB Function: '<S1>/Mean' */

  /* Sum: '<S1>/Add' incorporates:
   *  Constant: '<S1>/Offset'
   *  Gain: '<S1>/Gain'
   *  Gain: '<S1>/Gain1'
   *  MinMax: '<S1>/Max'
   *  Sum: '<S1>/Sum'
   */
  sum_val = ((real32_T)(52429U * maxV) * 7.62939453E-6F + 0.6F * sum_val) +
    (real32_T)Offset;
  for (k = 0; k < 12; k++) {
    /* DataTypeConversion: '<S1>/Data Type Conversion' incorporates:
     *  Inport: '<Root>/cell_voltage'
     *  RelationalOperator: '<S1>/Relational Operator'
     */
    cell_balance[k] = (uint8_T)(cell_voltage[k] > sum_val);
  }
}

/* Model initialize function */
void balance_initialize(void)
{
  /* (no initialization code required) */
}

/*
 * File trailer for generated code.
 *
 * [EOF]
 */
