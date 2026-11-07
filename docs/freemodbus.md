# FreeModbus 源码导读

这份库是 Modbus 从机。本工程只用 RTU、功能码 `0x03` 和 `0x06`。寄存器表、引脚和线程安排在 `docs/modbus.md`。这里只讲源码怎么把一帧请求变成一帧应答。

ASCII、TCP、线圈、离散量、输入寄存器可以不看。`demo/` 是别的芯片上的例子，串口和定时器是空的，也不用看。

要读的文件：

| 文件 | 看什么 |
| --- | --- |
| `modbus/mb.c` | 初始化、`eMBPoll()`、功能码表 |
| `modbus/rtu/mbrtu.c` | 按字节收发，用空闲时间判断一帧结束 |
| `modbus/rtu/mbcrc.c` | CRC16 |
| `modbus/functions/mbfuncholding.c` | `0x03`、`0x06` 怎么调用寄存器回调 |
| `modbus/functions/mbutils.c` | 回调的错误码怎么变成异常帧 |
| `modbus/include/mbport.h` | 移植层要提供的函数 |

协议栈不碰具体芯片。收到一字节、发送寄存器空了、定时器到了，都由移植层调用三个函数指针。这三个指针在 `eMBInit()` 里指向 RTU 的状态机。

## 一帧怎么走

主站发出 `01 03 00 00 00 05 ...` 之后，从机内部是这条顺序：

```text
串口中断，每收到 1 字节
    xMBRTUReceiveFSM() 放进 ucRTUBuf，并重新打开 1750 µs 定时器
定时器到期
    xMBRTUTimerT35Expired() 投递 EV_FRAME_RECEIVED
mb 线程第一次 eMBPoll()
    校验长度和 CRC，地址是本机，再投递 EV_EXECUTE
mb 线程第二次 eMBPoll()
    按功能码调用处理函数，处理函数调用 eMBRegHoldingCB()
    eMBRTUSend() 打开发送中断
发送中断，每空出一个字节
    xMBRTUTransmitFSM() 送出下一字节
全部送完
    投递 EV_FRAME_SENT，第三次 eMBPoll() 看到它就返回
```

`eMBPoll()` 没有事件就立刻返回。所以线程里可以写成调用一次再 `osDelay(1)`。一次调用只处理一个事件。收完帧和真正执行功能码是前后两次调用，帧指针保存在 `eMBPoll()` 的静态变量里。

事件只有一格，后来的会盖掉先来的。`demo/BARE/port/portevent.c` 就是一个标志加一个枚举。轮询停太久，下一帧会把 `EV_FRAME_RECEIVED` 或 `EV_EXECUTE` 盖掉，这一帧就没有应答。

## 初始化和启动

应用侧只做三步：

```c
eMBInit(MB_RTU, 1, 0, 115200, MB_PAR_NONE);
eMBEnable();
/* 之后反复调用 eMBPoll() */
```

`eMBInit()` 先拒绝地址 0 和大于 247 的地址。地址 0 是广播，从机不能把自己设成广播。然后把一组函数指针指到 `eMBRTUStart`、`eMBRTUReceive`、`eMBRTUSend`、`xMBRTUReceiveFSM`、`xMBRTUTransmitFSM`、`xMBRTUTimerT35Expired`，再调用 `eMBRTUInit()`。

`eMBRTUInit()` 做两件移植层的事：

- `xMBPortSerialInit()` 准备 8 位数据位的串口。本工程 USART1 已经按 115200 8N1 配好，这个函数确认后返回即可。端口号参数在这里用不上。
- `xMBPortTimersInit()` 准备帧间隔定时器。波特率大于 19200 时，间隔固定为 35 个 50 µs，也就是 1750 µs。源码注释写成 1800 µs，计数是 35。115200 走这条固定值。

此时状态是 `STATE_DISABLED`，还不应答。`eMBEnable()` 调用 `eMBRTUStart()`：接收状态设为 `STATE_RX_INIT`，只打开接收中断，并启动一次定时器。这 1750 µs 内如果总线一直空闲，定时器到期后才进入 `STATE_RX_IDLE`，并投递 `EV_READY`。`eMBPoll()` 对 `EV_READY` 什么都不做。它的作用是上电后先确认总线安静，再开始收新帧。

## 接收

RTU 没有帧头帧尾字节。一帧的结束靠「安静了 1750 µs」。

`xMBRTUReceiveFSM()` 在接收中断里运行。它先把字节读走，再按状态处理：

| 状态 | 收到字节时 |
| --- | --- |
| `STATE_RX_INIT` | 丢掉，重新定时。启动阶段还不算一帧 |
| `STATE_RX_IDLE` | 这是新帧的第一字节，写入 `ucRTUBuf[0]`，进入 `STATE_RX_RCV` |
| `STATE_RX_RCV` | 继续往 `ucRTUBuf` 里追加，最多 256 字节 |
| `STATE_RX_ERROR` | 这一帧已经超长，后续字节只用来重新定时，不再保存 |

每收到一字节都调用 `vMBPortTimersEnable()`，把 1750 µs 计数清零重来。字节间隔小于这个时间就一直算同一帧。安静时间到了，`xMBRTUTimerT35Expired()` 把接收状态改回 `STATE_RX_IDLE`。如果刚才停在 `STATE_RX_RCV`，就投递 `EV_FRAME_RECEIVED`。

缓冲 `ucRTUBuf[256]` 里的布局是：

```text
[0] 从机地址
[1] 功能码          ← eMBPoll 看到的 PDU 从这里开始
[2] 起的数据
[n-2] [n-1] CRC，低字节在前
```

`eMBRTUReceive()` 在任务里做两道检查：长度至少 4 字节（地址、功能码、两字节 CRC），并且对整段缓冲算 CRC 的结果是 0。通过之后，它把地址、PDU 指针和 PDU 长度交给 `eMBPoll()`。PDU 长度不含地址和 CRC。CRC 不对或太短就返回 `MB_EIO`，`eMBPoll()` 丢弃这一帧，不回异常。异常帧只用于「帧是合法的，但功能或寄存器不对」。

地址不是本机、也不是 0，同样丢弃。地址 0 的广播会执行功能，但不发送应答。

## 轮询里怎么执行功能码

`mb.c` 里有一张表 `xFuncHandlers[]`，编译期按 `mbconfig.h` 填进去。本方案只打开读保持寄存器和写单个保持寄存器，表里就是功能码 3 和功能码 6。表后面的空项功能码是 0，查找时遇到 0 就停止。

`EV_EXECUTE` 取出 PDU 第一个字节当功能码。对上了就调用处理函数。对不上，异常码保持 `MB_EX_ILLEGAL_FUNCTION`（数值 1）。

处理函数返回 `MB_EX_NONE` 时，它已经把应答 PDU 写回同一块缓冲，并改好长度。返回其它值时，`eMBPoll()` 把应答改成两字节：`功能码 | 0x80`，再加上异常码。所以主站看到功能码 `0x83` 就是读失败，`0x86` 就是写失败。

然后调用 `eMBRTUSend()`。如果这时接收状态已经不是 `STATE_RX_IDLE`，说明处理太慢，主站的下一帧已经开始进来，这次应答放弃，返回 `MB_EIO`。

## 功能码 0x03 和 0x06

两个功能码最后都进同一个回调：

```c
eMBErrorCode eMBRegHoldingCB(UCHAR *pucRegBuffer, USHORT usAddress,
                             USHORT usNRegs, eMBRegisterMode eMode);
```

`eMode` 为 `MB_REG_READ` 或 `MB_REG_WRITE`。这个函数由应用提供，协议栈里没有寄存器数组。

帧里的寄存器地址从 0 开始。`mbfuncholding.c` 解析后会先加 1，再传给回调。主站读帧内地址 0 时，回调里的 `usAddress` 是 1。本方案的对应关系：

| 帧内地址 | 回调里的 usAddress | 含义 |
| --- | --- | --- |
| 0～4 | 1～5 | 读：总电压、电流、SOC、充电 MOS、放电 MOS |
| 5 | 6 | 写：0 待机，1 充电，2 放电，3 Clear |

读请求的 PDU 固定 5 字节：功能码、地址高、地址低、数量高、数量低。数量要在 1 到 125 之间。应答在原缓冲上改写：先放功能码 `0x03`，再放字节数 `数量 × 2`，然后回调按大端填每个寄存器。缓冲里原来的地址和数量会被盖掉，这是预期行为。

写单个寄存器的 PDU 也是 5 字节：功能码、地址、值。数量固定为 1。成功时处理函数不改缓冲，`eMBRTUSend()` 会在前面加从机地址、后面重算 CRC，所以线上看到的应答和请求相同。

回调的返回值经过 `prveMBError2Exception()`：

| 回调返回 | 主站看到的异常 |
| --- | --- |
| `MB_ENOERR` | 正常应答 |
| `MB_ENOREG` | 02，非法地址 |
| `MB_ETIMEDOUT` | 06，从机忙 |
| `MB_EINVAL` 以及其余值 | 04，从机设备故障 |

非法命令值若要让主站看到异常 03，不能只返回 `MB_EINVAL`。这份函数会把它归进 04。

`mbfuncholding.c` 读数量时，低字节用 `|=` 拼进高字节。原来只赋值的那一行留在注释里。数量可以超过 255。

## 发送

`eMBRTUSend()` 不拷贝一份新缓冲。PDU 就放在 `ucRTUBuf[1]` 起的位置，它把从机地址写进 `ucRTUBuf[0]`，在末尾追加 CRC 低字节和高字节，然后只打开发送中断、关掉接收中断。

`xMBRTUTransmitFSM()` 在「发送寄存器空」中断里运行。计数还没到 0 就送出一个字节。计数到 0 的那一次中断不再送数据，而是投递 `EV_FRAME_SENT`，关掉发送、打开接收，状态回到空闲。因此一帧的最后一个字节发出去之后，还会再进一次发送中断。

## CRC

`usMBCRC16()` 用两张 256 字节的表计算 Modbus CRC16，初值 `0xFFFF`。返回值的高字节是 CRC 的高位。

发送时先写低字节再写高字节。接收时把地址、PDU 和这两字节 CRC 一起送进同一个函数。数据正确时，结果是 0。所以 `eMBRTUReceive()` 只判断 `usMBCRC16(...) == 0`，不再单独比较两个 CRC 字节。

## 移植层要接住的调用

`mbport.h` 里和本方案有关的是三类。

串口：

- `xMBPortSerialInit()`：确认 USART1 可用。
- `vMBPortSerialEnable(rx, tx)`：开关 RXNE 和 TXE 中断。接收和发送不会同时打开。
- `xMBPortSerialGetByte()`：读走 `DR`。接收状态机调用它之前，字节必须还在寄存器里。
- `xMBPortSerialPutByte()`：写一个字节到 `DR`。

定时器：

- `xMBPortTimersInit(35)`：配好 1750 µs 的单次定时。
- `vMBPortTimersEnable()`：计数清零并开始。每个新字节都会调用它。
- `vMBPortTimersDisable()`：停掉。
- 到期时调用 `pxMBPortCBTimerExpired()`，也就是 `xMBRTUTimerT35Expired()`。

事件：

- `xMBPortEventInit()`、`xMBPortEventPost()`、`xMBPortEventGet()`。一格标志就够。这三个函数会被中断和 `eMBPoll()` 两边调用。

`port.h` 里还要给出 `BOOL`、`UCHAR`、`USHORT`、`ULONG`，以及 `ENTER_CRITICAL_SECTION()` / `EXIT_CRITICAL_SECTION()`。RTU 在初始化、启动、收帧校验和发送准备时会进出临界区，其中接收状态机本身在中断里跑。临界区要保存并恢复 `PRIMASK`，这样从中断里退出时不会把中断提前打开。

接收溢出要在移植层清掉。状态机只调用 `xMBPortSerialGetByte()`，它不管 ORE。ORE 不清，后面的 RXNE 不再出现。
