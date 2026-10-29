#include "usart1.h"
#include "os.h"
#include "store.h"
#include "cmsis_os.h"

extern osMessageQueueId_t txQueueHandle;

static uint8_t txActive[TX_FRAME_LEN];
static volatile uint8_t txBusy;
static uint8_t rx_byte;

static void TxQueue_Pump(uint8_t finished);
static void TxQueue_Post(const uint8_t *data, uint16_t len);
static void TxQueue_OnComplete(void);

#define RX_STR_MAX 64U
static uint8_t rx_str[RX_STR_MAX];
static uint8_t rx_len;
static uint8_t rx_discard;
static volatile uint8_t rx_ready;
void USART1_Init()
{
	HAL_UART_Receive_IT(&huart1, &rx_byte, 1); /* 打开 USART1 接收 */
}
static void TxQueue_Pump(uint8_t finished)
{
    TxFrame frame;
    uint8_t len;
    uint8_t guard;

    for (guard = 0U; guard < 4U; guard++)
    {
        uint32_t primask = __get_PRIMASK();

        __disable_irq();
        if (finished != 0U)
        {
            txBusy = 0U;
            finished = 0U;
        }
        if (txBusy != 0U || txQueueHandle == NULL ||
            osMessageQueueGet(txQueueHandle, &frame, NULL, 0) != osOK)
        {
            if (primask == 0U)
            {
                __enable_irq();
            }
            return;
        }
        len = frame.len;
        if (len == 0U || len > TX_FRAME_LEN)
        {
            if (primask == 0U)
            {
                __enable_irq();
            }
            continue;
        }
        memcpy(txActive, frame.data, len);
        txBusy = 1U;
        if (primask == 0U)
        {
            __enable_irq();
        }
        if (HAL_UART_Transmit_IT(&huart1, txActive, len) == HAL_OK)
        {
            return;
        }
        finished = 1U;
    }
    txBusy = 0U;
}

static void TxQueue_Post(const uint8_t *data, uint16_t len)
{
    TxFrame frame;

    if (data == NULL || len == 0U || len > TX_FRAME_LEN || txQueueHandle == NULL)
    {
        return;
    }
    frame.len = (uint8_t)len;
    memcpy(frame.data, data, len);
    if (osMessageQueuePut(txQueueHandle, &frame, 0, 0) != osOK)
    {
        return;
    }
    TxQueue_Pump(0U);
}

static void TxQueue_OnComplete(void)
{
    TxQueue_Pump(1U);
}

void USART1_SendTxStr(uint16_t soc, uint16_t total_mv, int16_t current)
{
    int len;
    char buf[TX_FRAME_LEN];
    const char *chg;
	const char *dsg;

    chg = (HAL_GPIO_ReadPin(GPIOB, CHG_POWER_EN_Pin) == GPIO_PIN_SET) ? "ON" : "OFF";
	dsg = (HAL_GPIO_ReadPin(GPIOB, DSG_POWER_EN_Pin) == GPIO_PIN_SET) ? "ON" : "OFF";

    len = sprintf(buf, "\r\n\r\n当前SOC: %u.%02u%%\r\n",
              (unsigned)(soc / 100U), (unsigned)(soc % 100U));
    if (len <= 0 || len >= (int)sizeof(buf))
    {
        return;
    }
	
	len += sprintf(buf + len, "电流: %+d\r\n", (int)current);
	if (len <= 0 || len >= (int)sizeof(buf))
	{
    return;
	}
	
	len += sprintf(buf + len, "总电压: %u mV\r\n", (unsigned)total_mv);
	if (len <= 0 || len >= (int)sizeof(buf))
	{
		return;
	}


    len += sprintf(buf + len, "充电MOS: %s\r\n", chg);
    if (len <= 0 || len >= (int)sizeof(buf))
    {
        return;
    }
	
	len += sprintf(buf + len, "放电MOS: %s\r\n", dsg);
    if (len <= 0 || len >= (int)sizeof(buf))
    {
        return;
    }


    TxQueue_Post((const uint8_t *)buf, (uint16_t)len);
}

void USART1_SendText(const char *text)
{
    int len;
    char buf[TX_FRAME_LEN];

    if (text == NULL)
    {
        return;
    }
    len = sprintf(buf, "%s\r\n", text);
    if (len <= 0 || len >= (int)sizeof(buf))
    {
        return;
    }
    TxQueue_Post((const uint8_t *)buf, (uint16_t)len);
}

void OnUartLine(const char *line)
{
    /* 在这里处理 line，它不含 \r、\n，末尾已有 \0 */
	if(strcmp(line, "D") == 0)
	{
		StateRequest = Driving;
	} 
	else if(strcmp(line, "C") == 0)
	{
		StateRequest = Charging;
	} 
	else if(strcmp(line, "S") == 0) 
	{
		StateRequest = Standby;
//目前收到待机指令会将数据保存到flash
	} 
	else if(strcmp(line, "Clear") == 0) 
	{
		Store_Clear();
	
	}
}

void RECEICE_DATA_DEAL(void)
{
    char local[RX_STR_MAX];
    if (!rx_ready)
    {
        return;
    }
    memcpy(local, rx_str, RX_STR_MAX);
    rx_len = 0;
    rx_ready = 0;
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
    OnUartLine(local);
}


void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    TxQueue_OnComplete(); /* 发送完毕 */
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    uint8_t byte;

    if (huart->Instance != USART1)
    {
        return;
    }

    byte = rx_byte;
    if (byte == '\r')
    {
    }
    else if (byte == '\n')
    {
        if (rx_discard || rx_len == 0U)
        {
            rx_len = 0;
            rx_discard = 0;
        }
        else
        {
            rx_str[rx_len] = '\0';
            Control_PostLine((const char *)rx_str);
            rx_len = 0;
        }
    }
    else if (rx_discard || (rx_len + 1U >= RX_STR_MAX))
    {
        rx_len = 0;
        rx_discard = 1;
    }
    else
    {
        rx_str[rx_len++] = byte;
    }

    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1)
    {
        return;
    }
    if (huart->gState != HAL_UART_STATE_BUSY_TX)
    {
        TxQueue_OnComplete();
    }
    rx_len = 0;
    rx_discard = 0;
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
}
