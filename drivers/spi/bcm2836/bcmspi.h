/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

Module Name: 

    bcmspi.h

Abstract:

    This module contains the controller-specific type 
    definitions for the BCM2838 SPI controller driver hardware.

Environment:

    kernel-mode only

Revision History:

--*/

//
// Includes for hardware register definitions.
//

#ifndef _BCMSPI_H_
#define _BCMSPI_H_

//
// Broadcom SPI controller registers.
//

// The BCM2836 has three SPI controllers. 
// SPI0 is a full controller,
// SPI1/2 are mini SPI controllers embedded in the AUX block
// this driver implements the SPI0 controller 

// define SPI register map

typedef struct BCM_SPI_REGISTERS
{
    __declspec(align(4)) ULONG CS;        // SPI Master Control and Status
    __declspec(align(4)) ULONG FIFO;      // SPI Master TX and RX FIFOs
    __declspec(align(4)) ULONG CLK;       // SPI Master Clock Divider
    __declspec(align(4)) ULONG DLEN;      // SPI Master Data Length
    __declspec(align(4)) ULONG LTOH;      // SPI LOSSI mode TOH
    __declspec(align(4)) ULONG DC;        // SPI DMA DREQ Controls
}
BCM_SPI_REGISTERS, *PBCM_SPI_REGISTERS;

//
// CS register bits.
//

#define BCM_SPI_REG_CS_LEN_LONG     0x02000000
#define BCM_SPI_REG_CS_DMA_LEN      0x01000000
#define BCM_SPI_REG_CS_CSPOL2       0x00800000
#define BCM_SPI_REG_CS_CSPOL1       0x00400000
#define BCM_SPI_REG_CS_CSPOL0       0x00200000
#define BCM_SPI_REG_CS_RXF          0x00100000
#define BCM_SPI_REG_CS_RXR          0x00080000
#define BCM_SPI_REG_CS_TXD          0x00040000
#define BCM_SPI_REG_CS_RXD          0x00020000
#define BCM_SPI_REG_CS_DONE         0x00010000
#define BCM_SPI_REG_CS_TE_EN        0x00008000
#define BCM_SPI_REG_CS_LMONO        0x00004000
#define BCM_SPI_REG_CS_LEN          0x00002000
#define BCM_SPI_REG_CS_REN          0x00001000
#define BCM_SPI_REG_CS_ADCS         0x00000800
#define BCM_SPI_REG_CS_INTR         0x00000400
#define BCM_SPI_REG_CS_INTD         0x00000200
#define BCM_SPI_REG_CS_DMAEN        0x00000100
#define BCM_SPI_REG_CS_TA           0x00000080
#define BCM_SPI_REG_CS_CSPOL        0x00000040
#define BCM_SPI_REG_CS_CLEARRX      0x00000020
#define BCM_SPI_REG_CS_CLEARTX      0x00000010
#define BCM_SPI_REG_CS_CPOL         0x00000008
#define BCM_SPI_REG_CS_CPHA         0x00000004
#define BCM_SPI_REG_CS_CS           0x00000003
#define BCM_SPI_REG_CS_CS_SET(v)    (v & BCM_SPI_REG_CS_CS)

#define BCM_SPI_REG_CS_MODE_MASK    (BCM_SPI_REG_CS_CPOL | BCM_SPI_REG_CS_CPHA)

// default setting for polling mode, TA=0, CS active low, INT off, No DMA, clear Fifos, CS=0
#define BCM_SPI_REG_CS_POLL_DEFAULT     0 
#define BCM_SPI_REG_CS_FIFO_RESET       (BCM_SPI_REG_CS_CLEARRX | BCM_SPI_REG_CS_CLEARTX)

// from BCM2835 ARM peripherals 10.6.2
#define BCM_SPI_FIFO_BYTE_SIZE              16
#define BCM_SPI_DATA_BIT_LENGTH_SUPPORTED   8
#define BCM_SPI_CS_SUPPORTED                3

//
// CLK register bits.
//

#define BCM_APB_CLK                     250000000L  // 250Mhz core clock
#define BCM_SPI_REG_CLK_DEFAULT         100000      // 100 khz default SPI clock speed
#define BCM_SPI_REG_CLK_CDIV            0x0000ffff
#define BCM_SPI_REG_CLK_CDIV_MAX        0x0000fffe  // 3814Hz is the lowest clock with even divider
#define BCM_SPI_REG_CLK_CDIV_MIN        0x00000002  // 125Mhz is the highest clock with even divider
#define BCM_SPI_REG_CLK_CDIV_SET(v)     ((v) & BCM_SPI_REG_CLK_CDIV)
#define BCM_SPI_CLK_MAX_HZ              (BCM_APB_CLK / BCM_SPI_REG_CLK_CDIV_MIN)
#define BCM_SPI_CLK_MIN_HZ              (BCM_APB_CLK / BCM_SPI_REG_CLK_CDIV_MAX)
#define BCM_SPI_FIFO_FLUSH_TIMEOUT_US   \
    ((BCM_SPI_FIFO_BYTE_SIZE * BCM_SPI_DATA_BIT_LENGTH_SUPPORTED * 1000000) / BCM_SPI_CLK_MIN_HZ)

//
// DLEN register bits.
//

#define BCM_SPI_REG_DLEN_LEN        0x0000ffff
#define BCM_SPI_REG_DLEN_LEN_SET(v) (v & BCM_SPI_REG_DLEN_LEN)   

//
// LTOH register bits.
//

#define BCM_SPI_REG_LTOH_TOF        0x000000ff
#define BCM_SPI_REG_LTOH_TOF_SET(v) (v & BCM_SPI_REG_LTOH_TOF)   

//
// DC register bits.
//

#define BCM_SPI_REG_DC_RPANIC           0xff000000
#define BCM_SPI_REG_DC_RPANIC_SET(v)    (((v) << 24) & BCM_SPI_REG_DC_RPANIC)   
#define BCM_SPI_REG_DC_RDREQ            0x00ff0000
#define BCM_SPI_REG_DC_RDREQ_SET(v)     (((v) << 16) & BCM_SPI_REG_DC_RDREQ)   
#define BCM_SPI_REG_DC_TPANIC           0x0000ff00
#define BCM_SPI_REG_DC_TPANIC_SET(v)    (((v) << 8) & BCM_SPI_REG_DC_TPANIC)   
#define BCM_SPI_REG_DC_TDREQ            0x000000ff
#define BCM_SPI_REG_DC_TDREQ_SET(v)     ((v) & BCM_SPI_REG_DC_TDREQ)

// Number of clocks it takes the SPI HW to clock 1 byte
// The SPI HW waits an extra clock after each byte transfered
#define BCM_SPI_SCLK_TICKS_PER_BYTE 9

#endif

