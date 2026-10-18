/*
 * File: Ah_Integrator_Estimator.c
 *
 * Code generated for Simulink model 'Ah_Integrator_Estimator'.
 *
 * Model version                  : 1.43
 * Simulink Coder version         : 23.2 (R2023b) 01-Aug-2023
 * C/C++ source code generated on : Sun Oct 18 12:13:36 2026
 *
 * Target selection: ert.tlc
 * Embedded hardware selection: ARM Compatible->ARM Cortex-M
 * Code generation objectives: Unspecified
 * Validation result: Not run
 */

#include "Ah_Integrator_Estimator.h"
#include <math.h>
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
uint8_T SOC;                           /* '<S1>/single->uint8' */

/* Exported block parameters */
uint16_T Q_rated = 1200U;              /* Variable: Q_rated
                                        * Referenced by: '<S1>/总容量参数-mA'
                                        */
uint8_T eta_coulomb = 1U;              /* Variable: eta_coulomb
                                        * Referenced by: '<S1>/库伦效率系数'
                                        */

/* Block states (default storage) */
DW_Ah_Integrator_Estimator_T Ah_Integrator_Estimator_DW;

/* Real-time model */
static RT_MODEL_Ah_Integrator_Estima_T Ah_Integrator_Estimator_M_;
RT_MODEL_Ah_Integrator_Estima_T *const Ah_Integrator_Estimator_M =
  &Ah_Integrator_Estimator_M_;

/* Model step function */
void Ah_Integrator_Estimator_step(void)
{
  real32_T tmp;

  /* DataTypeConversion: '<S1>/single->uint8' incorporates:
   *  Constant: '<S1>/总容量参数-mA'
   *  DiscreteIntegrator: '<S1>/Discrete-Time Integrator'
   *  Gain: '<S1>/mAh->mAs'
   *  Gain: '<S1>/小数转百分比'
   *  Gain: '<S1>/百分比转小数'
   *  Inport: '<Root>/InitialSOC'
   *  Product: '<S1>/容量除法器'
   *  Sum: '<S1>/SOC加法器'
   */
  tmp = roundf((Ah_Integrator_Estimator_DW.DiscreteTimeIntegrator_DSTATE /
                (3600.0F * (real32_T)Q_rated) + 0.0100097656F * (real32_T)
                InitialSOC) * 100.0F);
  if (tmp < 256.0F) {
    if (tmp >= 0.0F) {
      /* DataTypeConversion: '<S1>/single->uint8' */
      SOC = (uint8_T)tmp;
    } else {
      /* DataTypeConversion: '<S1>/single->uint8' */
      SOC = 0U;
    }
  } else {
    /* DataTypeConversion: '<S1>/single->uint8' */
    SOC = MAX_uint8_T;
  }

  /* End of DataTypeConversion: '<S1>/single->uint8' */

  /* Update for DiscreteIntegrator: '<S1>/Discrete-Time Integrator' incorporates:
   *  Constant: '<S1>/库伦效率系数'
   *  Inport: '<Root>/Current'
   *  Product: '<S1>/Product'
   */
  Ah_Integrator_Estimator_DW.DiscreteTimeIntegrator_DSTATE += (real32_T)
    (eta_coulomb * Current) * 0.01F;
}

/* Model initialize function */
void Ah_Integrator_Estimator_initialize(void)
{
  /* (no initialization code required) */
}

/*
 * File trailer for generated code.
 *
 * [EOF]
 */
