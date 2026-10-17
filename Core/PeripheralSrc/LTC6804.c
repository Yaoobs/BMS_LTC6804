#include "LTC6804.h"
#include "spi.h"

/* PA4 软件片选 */
#define LTC6804_CS_high  HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET)
#define LTC6804_CS_low   HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET)

/* precomputed CRC15 Table */
static const unsigned int crc15Table[256] =
{
    0x0, 0xc599, 0xceab, 0xb32, 0xd8cf, 0x1d56, 0x1664, 0xd3fd,
    0xf407, 0x319e, 0x3aac, 0xff35, 0x2cc8, 0xe951, 0xe263, 0x27fa,
    0xad97, 0x680e, 0x633c, 0xa6a5, 0x7558, 0xb0c1, 0xbbf3, 0x7e6a,
    0x5990, 0x9c09, 0x973b, 0x52a2, 0x815f, 0x44c6, 0x4ff4, 0x8a6d,
    0x5b2e, 0x9eb7, 0x9585, 0x501c, 0x83e1, 0x4678, 0x4d4a, 0x88d3,
    0xaf29, 0x6ab0, 0x6182, 0xa41b, 0x77e6, 0xb27f, 0xb94d, 0x7cd4,
    0xf6b9, 0x3320, 0x3812, 0xfd8b, 0x2e76, 0xebef, 0xe0dd, 0x2544,
    0x2be, 0xc727, 0xcc15, 0x98c, 0xda71, 0x1fe8, 0x14da, 0xd143,
    0xf3c5, 0x365c, 0x3d6e, 0xf8f7, 0x2b0a, 0xee93, 0xe5a1, 0x2038,
    0x7c2, 0xc25b, 0xc969, 0xcf0, 0xdf0d, 0x1a94, 0x11a6, 0xd43f,
    0x5e52, 0x9bcb, 0x90f9, 0x5560, 0x869d, 0x4304, 0x4836, 0x8daf,
    0xaa55, 0x6fcc, 0x64fe, 0xa167, 0x729a, 0xb703, 0xbc31, 0x79a8,
    0xa8eb, 0x6d72, 0x6640, 0xa3d9, 0x7024, 0xb5bd, 0xbe8f, 0x7b16,
    0x5cec, 0x9975, 0x9247, 0x57de, 0x8423, 0x41ba, 0x4a88, 0x8f11,
    0x57c, 0xc0e5, 0xcbd7, 0xe4e, 0xddb3, 0x182a, 0x1318, 0xd681,
    0xf17b, 0x34e2, 0x3fd0, 0xfa49, 0x29b4, 0xec2d, 0xe71f, 0x2286,
    0xa213, 0x678a, 0x6cb8, 0xa921, 0x7adc, 0xbf45, 0xb477, 0x71ee,
    0x5614, 0x938d, 0x98bf, 0x5d26, 0x8edb, 0x4b42, 0x4070, 0x85e9,
    0xf84, 0xca1d, 0xc12f, 0x4b6, 0xd74b, 0x12d2, 0x19e0, 0xdc79,
    0xfb83, 0x3e1a, 0x3528, 0xf0b1, 0x234c, 0xe6d5, 0xede7, 0x287e,
    0xf93d, 0x3ca4, 0x3796, 0xf20f, 0x21f2, 0xe46b, 0xef59, 0x2ac0,
    0xd3a, 0xc8a3, 0xc391, 0x608, 0xd5f5, 0x106c, 0x1b5e, 0xdec7,
    0x54aa, 0x9133, 0x9a01, 0x5f98, 0x8c65, 0x49fc, 0x42ce, 0x8757,
    0xa0ad, 0x6534, 0x6e06, 0xab9f, 0x7862, 0xbdfb, 0xb6c9, 0x7350,
    0x51d6, 0x944f, 0x9f7d, 0x5ae4, 0x8919, 0x4c80, 0x47b2, 0x822b,
    0xa5d1, 0x6048, 0x6b7a, 0xaee3, 0x7d1e, 0xb887, 0xb3b5, 0x762c,
    0xfc41, 0x39d8, 0x32ea, 0xf773, 0x248e, 0xe117, 0xea25, 0x2fbc,
    0x846, 0xcddf, 0xc6ed, 0x374, 0xd089, 0x1510, 0x1e22, 0xdbbb,
    0xaf8, 0xcf61, 0xc453, 0x1ca, 0xd237, 0x17ae, 0x1c9c, 0xd905,
    0xfeff, 0x3b66, 0x3054, 0xf5cd, 0x2630, 0xe3a9, 0xe89b, 0x2d02,
    0xa76f, 0x62f6, 0x69c4, 0xac5d, 0x7fa0, 0xba39, 0xb10b, 0x7492,
    0x5368, 0x96f1, 0x9dc3, 0x585a, 0x8ba7, 0x4e3e, 0x450c, 0x8095
};

void wakeup_idle(void)
{
    LTC6804_CS_low;
    HAL_Delay(1);
    LTC6804_CS_high;
}

/* len: number of bytes that will be used to calculate a PEC
 * data: array of data that will be used to calculate a PEC
 */
uint16_t pec15_calc(uint8_t len, uint8_t *data)
{
    uint16_t remainder;
    uint16_t addr;
    uint8_t i;

    remainder = 16; /* initialize the PEC */
    for (i = 0; i < len; i++) /* loops for each byte in data array */
    {
        addr = ((remainder >> 7) ^ data[i]) & 0xff; /* calculate PEC table address */
        remainder = (remainder << 8) ^ crc15Table[addr];
    }
    return (remainder * 2); /* The CRC15 has a 0 in the LSB so the remainder must be multiplied by 2 */
}

void LTC6804_initialize(void)
{
    uint8_t cfg[6];

    wakeup_idle();
    HAL_Delay(10);
    cfg[0] = 0xFC; /* GPIO=1，REFON=1，ADCOPT=0 */
    cfg[1] = 0x00;
    cfg[2] = 0x00;
    cfg[3] = 0x00;
    cfg[4] = 0x00;
    cfg[5] = 0x00;
    LTC6804_wrcfg(cfg);
}

/* tx_Data: array of data to be written on SPI port
 * tx_len: length of the tx data array
 * rx_data: array that will store the data read by the SPI port
 * rx_len: number of bytes to be read from the SPI port
 */
void spi_write_read(uint8_t tx_Data[], uint8_t tx_len, uint8_t *rx_data, uint8_t rx_len)
{
    uint8_t i;
    uint8_t data[rx_len];

    HAL_SPI_Transmit(&hspi1, tx_Data, tx_len, 2);
    for (i = 0; i < rx_len; i++)
    {
        data[i] = 0x00;
    }
    HAL_SPI_TransmitReceive(&hspi1, data, rx_data, rx_len, 2);
}

/* len: number of bytes to be written on the SPI port
 * data: array of bytes to be written on the SPI port
 */
void spi_write_array(uint8_t len, uint8_t data[])
{
    HAL_SPI_Transmit(&hspi1, data, len, 2);
}

/*!*******************************************************************************************************************

Command Code:
-------------

|command    |  15   |  14   |  13   |  12   |  11   |  10   |   9   |   8   |   7   |   6   |   5   |   4   |   3   |   2   |   1   |   0   |
|-----------|-------|-------|-------|-------|-------|-------|-------|-------|-------|-------|-------|-------|-------|-------|-------|-------|
|ADCV:      |   0   |   0   |   0   |   0   |   0   |   0   |   1   | MD[1] | MD[2] |   1   |   1   |  DCP  |   0   | CH[2] | CH[1] | CH[0] |
|ADAX:      |   0   |   0   |   0   |   0   |   0   |   1   |   0   | MD[1] | MD[2] |   1   |   1   |  DCP  |   0   | CHG[2]| CHG[1]| CHG[0]|
 ******************************************************************************************************************/
void LTC6804_adcv(void)
{
    uint8_t cmd[4];
    uint16_t cmd_pec;

    cmd[0] = 0x03;
    cmd[1] = 0x60;
    cmd_pec = pec15_calc(2, cmd);
    cmd[2] = (uint8_t)(cmd_pec >> 8);
    cmd[3] = (uint8_t)(cmd_pec);

    LTC6804_CS_low;
    spi_write_array(4, cmd);
    LTC6804_CS_high;
}

/* reg: determines which cell voltage register is read back
 * data: an array of the unparsed cell codes
 */
void LTC6804_rdcv_reg(uint8_t reg, uint8_t *data)
{
    const uint8_t REG_LEN = 8; /* number of bytes in each ICs register + 2 bytes for the PEC */
    uint8_t cmd[4];
    uint16_t cmd_pec;

    if (reg == 1) /* 1: RDCVA */
    {
        cmd[1] = 0x04;
        cmd[0] = 0x00;
    }
    else if (reg == 2) /* 2: RDCVB */
    {
        cmd[1] = 0x06;
        cmd[0] = 0x00;
    }
    else if (reg == 3) /* 3: RDCVC */
    {
        cmd[1] = 0x08;
        cmd[0] = 0x00;
    }
    else if (reg == 4) /* 4: RDCVD */
    {
        cmd[1] = 0x0A;
        cmd[0] = 0x00;
    }

    cmd_pec = pec15_calc(2, cmd);
    cmd[2] = (uint8_t)(cmd_pec >> 8);
    cmd[3] = (uint8_t)(cmd_pec);

    LTC6804_CS_low;
    spi_write_read(cmd, 4, data, REG_LEN);
    LTC6804_CS_high;
}

/* raw 固定 8 字节。reg 取 1～4。
 * PEC 正确才写入对应的 3 节；失败则保持 cell_codes 里的旧值。
 * 返回 0：通过。返回 -1：参数非法或 PEC 失败。
 * 写入的仍是原始码，1 LSB = 100 μV。除以 10 得到毫伏留给 Get_Cell_Voltage()。
 */
int8_t ltc6804_parse_cell_group(const uint8_t raw[8], uint8_t reg, uint16_t cell_codes[12])
{
    uint8_t cell;
    uint16_t received_pec;
    uint16_t data_pec;

    if (raw == 0 || cell_codes == 0 || reg < 1 || reg > 4)
    {
        return -1;
    }

    received_pec = ((uint16_t)raw[6] << 8) + raw[7];
    data_pec = pec15_calc(6, (uint8_t *)raw);
    if (received_pec != data_pec)
    {
        return -1;
    }

    for (cell = 0; cell < 3; cell++)
    {
        uint16_t code = raw[cell * 2] + ((uint16_t)raw[cell * 2 + 1] << 8);
        cell_codes[cell + (reg - 1) * 3] = code;
    }
    return 0;
}

int8_t LTC6804_rdcv(uint16_t cell_codes[12])
{
    uint8_t raw[8];
    uint8_t group;
    int8_t err = 0;

    for (group = 1; group <= 4; group++)
    {
        LTC6804_rdcv_reg(group, raw);
        if (ltc6804_parse_cell_group(raw, group, cell_codes) != 0)
        {
            err = -1;
        }
    }
    return err;
}

/* config: configuration data that will be written */
void LTC6804_wrcfg(uint8_t config[6])
{
    const uint8_t BYTES_IN_REG = 6;
    const uint8_t CMD_LEN = 12;
    uint8_t cmd[CMD_LEN];
    uint16_t cfg_pec;
    uint8_t cmd_index;
    uint8_t current_byte;

    cmd[0] = 0x00;
    cmd[1] = 0x01;
    cmd_pec = pec15_calc(2, cmd);
    cmd[2] = (uint8_t)(cmd_pec >> 8);
    cmd[3] = (uint8_t)(cmd_pec);

    cmd_index = 4;
    for (current_byte = 0; current_byte < BYTES_IN_REG; current_byte++)
    {
        cmd[cmd_index] = config[current_byte];
        cmd_index = cmd_index + 1;
    }

    cfg_pec = (uint16_t)pec15_calc(BYTES_IN_REG, &config[0]);
    cmd[cmd_index] = (uint8_t)(cfg_pec >> 8);
    cmd[cmd_index + 1] = (uint8_t)cfg_pec;
    cmd_index = cmd_index + 2;

    LTC6804_CS_low;
    spi_write_array(CMD_LEN, cmd);
    LTC6804_CS_high;
}

/* r_config: array that stores the read configuration data */
int8_t LTC6804_rdcfg(uint8_t r_config[8])
{
    const uint8_t BYTES_IN_REG = 8;
    uint8_t cmd[4];
    uint8_t rx_data[8];
    int8_t pec_error = 0;
    uint16_t data_pec;
    uint16_t received_pec;
    uint8_t current_byte;

    cmd[0] = 0x00;
    cmd[1] = 0x02;
    cmd_pec = pec15_calc(2, cmd);
    cmd[2] = (uint8_t)(cmd_pec >> 8);
    cmd[3] = (uint8_t)(cmd_pec);

    LTC6804_CS_low;
    spi_write_read(cmd, 4, rx_data, BYTES_IN_REG);
    LTC6804_CS_high;

    for (current_byte = 0; current_byte < BYTES_IN_REG; current_byte++)
    {
        r_config[current_byte] = rx_data[current_byte];
    }

    received_pec = (r_config[6] << 8) + r_config[7];
    data_pec = pec15_calc(6, &r_config[0]);
    if (received_pec != data_pec)
    {
        pec_error = -1;
    }

    return pec_error;
}
