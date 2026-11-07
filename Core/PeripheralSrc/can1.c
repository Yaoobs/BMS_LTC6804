#include "can1.h"
#include "can.h"

uint8_t CAN_RxBuf[8];   // 标准数据帧，最大 8 字节

/**
  * @brief  配置 CAN 滤波器
  * @note   设置 CAN 接收滤波器参数，仅允许接收标准 ID 为 0x123 的消息
  * @param  无
  * @retval 无
  */
void CAN_Filter_Config(void)
{
    CAN_FilterTypeDef can_filter = {0};           // 定义 CAN 滤波器结构体并清零

    can_filter.FilterBank = 0;                     // 使用第 0 个滤波器组
    can_filter.FilterMode = CAN_FILTERMODE_IDMASK; // 设置滤波器模式为掩码模式
    can_filter.FilterScale = CAN_FILTERSCALE_32BIT; // 32 位滤波器
    can_filter.FilterIdHigh     = (0x123 << 5);    // 高 16 位 ID，标准帧 ID 左移 5 位
    can_filter.FilterIdLow      = 0x0000;          // 标准帧下低 16 位为 0
    can_filter.FilterMaskIdHigh = (0x7FF << 5);    // 高 16 位掩码，仅匹配 11 位标准 ID
    can_filter.FilterMaskIdLow  = 0x0000;          // 低 16 位掩码为 0
    can_filter.FilterFIFOAssignment = CAN_FILTER_FIFO0; // 指定接收 FIFO0
    can_filter.FilterActivation = ENABLE;          // 激活滤波器

    if(HAL_CAN_ConfigFilter(&hcan, &can_filter) != HAL_OK) // 配置滤波器
    {
        Error_Handler();                           // 配置失败，执行错误处理
    }
}


/**
 * @brief  完整初始化CAN的功能
 * @param  无
 * @retval 无
 */
void CAN_Init(void)
{
    
    /* 配置 CAN 过滤器 */
    CAN_Filter_Config();
	
	/* 启动 CAN 外设 */
	HAL_CAN_Start(&hcan);

	/* 激活接收中断（FIFO0 消息挂起中断） */
	HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
	

}

/**
  * @brief  发送 CAN 数据
  * @param  StdId 标准 ID
  * @param  pData 指向发送数据的指针
  * @param  Len   发送数据长度（最大 8 字节）
  * @retval 无
  */
void CAN_Send(uint32_t StdId, uint8_t *pData, uint8_t Len)
{
    CAN_TxHeaderTypeDef txHeader;                 // 定义 CAN 发送报文头
    uint32_t txMailbox;                           // 保存发送邮箱编号

    txHeader.StdId = StdId;                       // 设置标准 ID
    txHeader.IDE = CAN_ID_STD;                    // 标准帧
    txHeader.RTR = CAN_RTR_DATA;                  // 数据帧
    txHeader.DLC = Len;                           // 数据长度

    if (HAL_CAN_AddTxMessage(&hcan, &txHeader, pData, &txMailbox) == HAL_OK)
    {
//        printf("CAN 发送数据成功!\r\n");
//        printf("发送的报文信息如下：\r\n");
//        printf("标准 ID：0x%03X\r\n", txHeader.StdId);
//        printf("帧类型：%s\r\n", (txHeader.RTR == CAN_RTR_DATA) ? "数据帧" : "远程帧");
    }
}

/**
  * @brief  CAN 接收 FIFO0 消息回调函数
  * @param  hcan 指向 CAN 外设句柄的指针
  * @retval 无
  */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef rxHeader;                 // 定义 CAN 接收报文头

    /* 读取接收 FIFO0 的消息 */
    HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rxHeader, CAN_RxBuf);

    /* 串口打印报文信息 */
//		printf("\r\n========================\r\n");
//    printf("CAN 接收到数据：\r\n");
    
    /* 判断帧类型（标准帧或扩展帧） */
//    if (rxHeader.IDE == CAN_ID_STD)
//        printf("标准帧 ID：0x%03X\r\n", rxHeader.StdId);
//    else
//        printf("扩展帧 ID：0x%08X\r\n", rxHeader.ExtId);

//    /* 打印帧类型和数据长度 */
//    printf("帧类型：%s\r\n", (rxHeader.RTR == CAN_RTR_DATA) ? "数据帧" : "远程帧");
//    printf("数据长度：%d 字节\r\n", rxHeader.DLC);

//    /* 打印数据内容 */
//    printf("数据内容：");
    for (uint8_t i = 0; i < rxHeader.DLC; i++)
    {
//        printf("%02X ", CAN_RxBuf[i]);
    }
//    printf("\r\n========================\r\n");
}
