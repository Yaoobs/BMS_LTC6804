#include "usart1.h"


#define TX_UART_HEAD_0  0xAAU
#define TX_UART_HEAD_1  0x01U
volatile uint8_t tx_busy;

static uint8_t cmd[3];
static uint8_t cmd_index;
static volatile uint8_t cmd_ready; /* 收到完整3个字节命令 */

#define RX_UART_LEN        3U
#define RX_UART_HEAD       0x01U
#define RX_UART_TAIL       0x55U
#define RX_UART_OPEN_DSG   0x04U
#define RX_UART_CLOSE_DSG  0x05U
#define RX_UART_OPEN_CHG   0x06U
#define RX_UART_CLOSE_CHG  0x07U

uint8_t rx_byte;
uint8_t tx_data[100];

#define TX_FRAME_LEN 50U

#define RX_STR_MAX 64U
static uint8_t rx_str[RX_STR_MAX];
static uint8_t rx_len;
static uint8_t rx_discard;
static volatile uint8_t rx_ready;

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
    int len;
    const char *chg;
	const char *dsg;

    chg = (HAL_GPIO_ReadPin(GPIOB, CHG_POWER_EN_Pin) == GPIO_PIN_SET) ? "ON" : "OFF";
	dsg = (HAL_GPIO_ReadPin(GPIOB, DSG_POWER_EN_Pin) == GPIO_PIN_SET) ? "ON" : "OFF";

    len = sprintf((char *)tx_data, "当前SOC: %u.%02u%%\r\n",
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

//void USART1_SendTxData() 
//{
//	uint8_t vi;
//	tx_data[0] = TX_UART_HEAD_0;
//    tx_data[1] = TX_UART_HEAD_1;
//    for (vi = 0; vi < 12; vi++)
//    {
//        tx_data[vi + 2 + vi] = (uint8_t)(cell_voltage[vi] >> 8);
//        tx_data[vi + 3 + vi] = (uint8_t)(cell_voltage[vi] & 0x00FF);
//    }
//    tx_data[32] = (uint8_t)(total_voltage >> 8);
//    tx_data[33] = (uint8_t)(total_voltage & 0xFF);
//    tx_data[34] = (uint8_t)(SOC >> 8);
//    tx_data[35] = (uint8_t)(SOC & 0xFF);
//    tx_data[36] = (uint8_t)(((uint16_t)Current) >> 8);
//    tx_data[37] = (uint8_t)(((uint16_t)Current) & 0x00FF);
//    tx_data[38] = temp;
//    tx_data[39] = HAL_GPIO_ReadPin(GPIOB, DSG_POWER_EN_Pin);
//    tx_data[40] = HAL_GPIO_ReadPin(GPIOB, CHG_POWER_EN_Pin);
//    tx_data[41] = (uint8_t)(Max >> 8);
//    tx_data[42] = (uint8_t)(Max & 0x00FF);
//    tx_data[43] = (uint8_t)(Min >> 8);
//    tx_data[44] = (uint8_t)(Min & 0x00FF);
//	USART1_SendData(tx_data, TX_FRAME_LEN);
//}

//void RECEICE_DATA_DEAL(void)
//{
//    uint8_t local[RX_UART_LEN];

//    if (!cmd_ready)
//    {
//        return;
//    }

//    local[0] = cmd[0];
//    local[1] = cmd[1];
//    local[2] = cmd[2];
//    cmd_ready = 0;
//    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);

//    if (local[0] != RX_UART_HEAD || local[2] != RX_UART_TAIL)
//    {
//        return;
//    }

//    switch (local[1])
//    {
//        case RX_UART_OPEN_DSG:
//            Only_Open_DSG();
//            break;
//        case RX_UART_CLOSE_DSG:
//            Only_Close_DSG();
//            break;
//        case RX_UART_OPEN_CHG:
//            Only_Open_CHG();
//            break;
//        case RX_UART_CLOSE_CHG:
//            Only_Close_CHG();
//            break;
//        default:
//            break;
//    }
//}

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

//void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
//{
//    if (huart->Instance != USART1)
//    {
//        return;
//    }

//    if (cmd_index == 0)
//    {
//        if (rx_byte == RX_UART_HEAD)
//        {
//            cmd[cmd_index++] = rx_byte;
//        }
//    }
//    else
//    {
//        cmd[cmd_index++] = rx_byte;
//        if (cmd_index == RX_UART_LEN)
//        {
//            cmd_index = 0;
//            if (cmd[2] == RX_UART_TAIL)
//            {
//                cmd_ready = 1;
//            }
//        }
//    }

//    if (!cmd_ready)
//    {
//        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
//    }
//}

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
            rx_ready = 1;
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

    if (!rx_ready)
    {
        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
    }
}

//void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
//{
//    if (huart->Instance != USART1)
//    {
//        return;
//    }

//    tx_busy = 0;
//    cmd_index = 0;
//    if (!cmd_ready)
//    {
//        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
//    }
//}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1)
    {
        return;
    }
    tx_busy = 0;
    rx_len = 0;
    rx_discard = 0;
    if (!rx_ready)
    {
        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
    }
}
