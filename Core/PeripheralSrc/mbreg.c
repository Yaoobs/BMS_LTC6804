#include "mb.h"
#include "os.h"
#include "sample.h"
#include "Ah_Integrator_Estimator.h"

static void Mb_PutU16(UCHAR *buf, USHORT value)
{
    buf[0] = (UCHAR)(value >> 8);
    buf[1] = (UCHAR)(value & 0xFFU);
}

static USHORT Mb_ReadReg(USHORT address)
{
    switch (address)
    {
    case 1U:
        return total_voltage;
    case 2U:
        return (USHORT)Current;
    case 3U:
        return SOC;
    case 4U:
        return (HAL_GPIO_ReadPin(CHG_POWER_EN_GPIO_Port, CHG_POWER_EN_Pin) == GPIO_PIN_SET)
                   ? 1U
                   : 0U;
    case 5U:
        return (HAL_GPIO_ReadPin(DSG_POWER_EN_GPIO_Port, DSG_POWER_EN_Pin) == GPIO_PIN_SET)
                   ? 1U
                   : 0U;
    default:
        return 0U;
    }
}

eMBErrorCode eMBRegHoldingCB(UCHAR *pucRegBuffer, USHORT usAddress,
                             USHORT usNRegs, eMBRegisterMode eMode)
{
    USHORT i;
    USHORT value;
    const char *line;

    if (eMode == MB_REG_WRITE)
    {
        if (usAddress != 6U || usNRegs != 1U)
        {
            return MB_ENOREG;
        }
        value = (USHORT)(((USHORT)pucRegBuffer[0] << 8) | pucRegBuffer[1]);
        if (value == 0U)
        {
            line = "S";
        }
        else if (value == 1U)
        {
            line = "C";
        }
        else if (value == 2U)
        {
            line = "D";
        }
        else if (value == 3U)
        {
            line = "Clear";
        }
        else
        {
            return MB_EINVAL;
        }
        if (Control_PostLine(line) != 0)
        {
            return MB_ETIMEDOUT;
        }
        return MB_ENOERR;
    }

    if (usNRegs == 0U || usAddress < 1U || usAddress > 5U ||
        usNRegs > (USHORT)(6U - usAddress))
    {
        return MB_ENOREG;
    }
    for (i = 0U; i < usNRegs; i++)
    {
        Mb_PutU16(pucRegBuffer + (i * 2U), Mb_ReadReg((USHORT)(usAddress + i)));
    }
    return MB_ENOERR;
}
