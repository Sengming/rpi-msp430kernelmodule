#ifndef MSP430SPI_H_
#define MSP430SPI_H_

#include <linux/types.h>

union msp430_spi_cs_register
{
    u32 full_buffer;
    struct {
        u8 csbits            : 2;
        u8 cpha              : 1;
        u8 cpol              : 1;
        u8 clear             : 2;
        u8 cspol             : 1;
        u8 transfer_active   : 1;
        u8 dma_en            : 1;
        u8 itr_on_done       : 1;
        u8 itr_on_rx         : 1;
        u8 auto_deass_cs     : 1;
        u8 read_en           : 1;
        u8 lossi_en          : 1;
        u8                   : 1; // Unused
        u8                   : 1; // Unused
        u8 transfer_done     : 1;
        u8 rx_has_data       : 1;
        u8 tx_accept_data    : 1;
        u8 rx_needs_read     : 1;
        u8 rx_full           : 1;
        u8 cs_pol_1          : 1;
        u8 cs_pol_2          : 1;
        u8 cs_pol_3          : 1;
        u8 dma_en_lossi      : 1;
        u8 en_long_lossi     : 1;
        u8                   : 6; // Reserved
    };
};

union msp430_spi_dc_register
{
    u32 full_buffer;
    struct {
        u8 dma_write_thresh     : 8;
        u8 dma_write_panic      : 8;
        u8 dma_read_thresh      : 8;
        u8 dma_read_panic       : 8;
    };
};

union msp430spi_clk_register
{
    u32 full_buffer;
    struct {
        u16 clk_div         : 16;
        u16                 : 16; // Reserved
    };
};

union msp430spi_dlen_register
{
    u32 full_buffer;
    struct {
        u16 data_len        : 16;
        u16                 : 16; // Reserved
    };
};

union msp430spi_ltoh_register
{
    u32 full_buffer;
    struct {
        u16 lossi_out_hold  : 16;
        u16                 : 16; // Reserved
    };
};

union msp430spi_fifo_register
{
    u32 full_buffer;
};
#endif
