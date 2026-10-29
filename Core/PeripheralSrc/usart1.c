#include "usart1.h"
#include "os.h"
#include "store.h"

static volatile uint8_t tx_busy;

static uint8_t rx_byte;
uint8_t tx_data[100];

#define TX_FRAME_LEN 50U

#define RX_STR_MAX 64U
static uint8_t rx_str[RX_STR_MAX];
static uint8_t rx_len;
static uint8_t rx_discard;
static volatile uint8_t rx_ready;
void USART1_Init()
{
	HAL_UART_Receive_IT(&huart1, &rx_byte, 1); /* 打开 USART1 接收 */
}
void USART1_SendData(const uint8_t *frame, uint16_t len)
{
    if (frame == NULL || len == 0)
    {
        return;
    }

    tx_busy = 1;
    if (HAL_UART_Transmit_IT(&huart1, (uint8_t *)frame, len) != HAL_OK)
    {
        tx_busy = 0;
    }
}

void USART1_SendTxStr(void)
{
	 if (tx_busy != 0)
    {
		return;
	}
    int len;
    const char *chg;
	const char *dsg;

    chg = (HAL_GPIO_ReadPin(GPIOB, CHG_POWER_EN_Pin) == GPIO_PIN_SET) ? "ON" : "OFF";
	dsg = (HAL_GPIO_ReadPin(GPIOB, DSG_POWER_EN_Pin) == GPIO_PIN_SET) ? "ON" : "OFF";

    len = sprintf((char *)tx_data, "\r\n\r\n当前SOC: %u.%02u%%\r\n",
              (unsigned)(SOC / 100U), (unsigned)(SOC % 100U));
    if (len <= 0 || len >= (int)sizeof(tx_data))
    {
        return;
    }
	
	len += sprintf((char *)tx_data + len, "电流: %+d\r\n", (int)Current);
	if (len <= 0 || len >= (int)sizeof(tx_data))
	{
    return;
	}
	
	len += sprintf((char *)tx_data + len, "总电压: %u mV\r\n", (unsigned)total_voltage);
	if (len <= 0 || len >= (int)sizeof(tx_data))
	{
		return;
	}


    len += sprintf((char *)tx_data + len, "充电MOS: %s\r\n", chg);
    if (len <= 0 || len >= (int)sizeof(tx_data))
    {
        return;
    }
	
	len += sprintf((char *)tx_data + len, "放电MOS: %s\r\n", dsg);
    if (len <= 0 || len >= (int)sizeof(tx_data))
    {
        return;
    }


    USART1_SendData(tx_data, (uint16_t)len);
}

void USART1_SendText(const char *text)
{
    int len;

    if (text == NULL || tx_busy != 0)
    {
        return;
    }
    len = sprintf((char *)tx_data, "%s\r\n", text);
    if (len <= 0 || len >= (int)sizeof(tx_data))
    {
        return;
    }
    USART1_SendData(tx_data, (uint16_t)len);
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
    tx_busy = 0; /* 发送完毕 */
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
    tx_busy = 0;
    rx_len = 0;
    rx_discard = 0;
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
}
