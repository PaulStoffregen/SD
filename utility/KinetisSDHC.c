// adapted for Arduino SD library by Paul Stoffregen
// following code is modified by Walter Zimmer from
// from version provided by
// Petr Gargulak (NXP Employee)
//https://community.nxp.com/servlet/JiveServlet/download/339474-1-263510/SDHC_K60_Baremetal.ZIP
//see also
//https://community.nxp.com/thread/99202

#if defined(__MK64FX512__) || defined(__MK66FX1M0__)

#include "kinetis.h"
//#include "core_pins.h" // testing only
//#include "HardwareSerial.h" // testing only

// Missing in Teensyduino 1.30
#ifndef MPU_CESR_VLD_MASK
#define MPU_CESR_VLD_MASK         0x1u
#endif

/******************************************************************************
* Constants
******************************************************************************/

enum {
        SDHC_RESULT_OK = 0,             /* 0: Successful */
        SDHC_RESULT_ERROR,              /* 1: R/W Error */
        SDHC_RESULT_WRPRT,              /* 2: Write Protected */
        SDHC_RESULT_NOT_READY,          /* 3: Not Ready */
        SDHC_RESULT_PARERR,             /* 4: Invalid Parameter */
        SDHC_RESULT_NO_RESPONSE         /* 5: No Response */ // from old diskio.h
};

/*void print_result(int n)
{
        switch (n) {
        case SDHC_RESULT_OK: serial_print("OK\n"); break;
        case SDHC_RESULT_ERROR: serial_print("R/W Error\n"); break;
        case SDHC_RESULT_WRPRT: serial_print("Write Protect\n"); break;
        case SDHC_RESULT_NOT_READY: serial_print("Not Ready\n"); break;
        case SDHC_RESULT_PARERR: serial_print("Invalid Param\n"); break;
        case SDHC_RESULT_NO_RESPONSE: serial_print("No Response\n"); break;
        default: serial_print("Unknown result\n");
        }
}*/

#define SDHC_STATUS_NOINIT              0x01    /* Drive not initialized */
#define SDHC_STATUS_NODISK              0x02    /* No medium in the drive */
#define SDHC_STATUS_PROTECT             0x04    /* Write protected */


#define IO_SDHC_ATTRIBS (IO_DEV_ATTR_READ | IO_DEV_ATTR_REMOVE | IO_DEV_ATTR_SEEK | IO_DEV_ATTR_WRITE | IO_DEV_ATTR_BLOCK_MODE)

#define SDHC_XFERTYP_RSPTYP_NO              (0x00)
#define SDHC_XFERTYP_RSPTYP_136             (0x01)
#define SDHC_XFERTYP_RSPTYP_48              (0x02)
#define SDHC_XFERTYP_RSPTYP_48BUSY          (0x03)

#define SDHC_XFERTYP_CMDTYP_ABORT           (0x03)

#define SDHC_PROCTL_EMODE_INVARIANT         (0x02)

#define SDHC_PROCTL_DTW_1BIT                (0x00)
#define SDHC_PROCTL_DTW_4BIT                (0x01)
#define SDHC_PROCTL_DTW_8BIT                (0x10)

#define SDHC_INITIALIZATION_MAX_CNT 100000

/* SDHC commands */
#define SDHC_CMD0                           (0)
#define SDHC_CMD1                           (1)
#define SDHC_CMD2                           (2)
#define SDHC_CMD3                           (3)
#define SDHC_CMD4                           (4)
#define SDHC_CMD5                           (5)
#define SDHC_CMD6                           (6)
#define SDHC_CMD7                           (7)
#define SDHC_CMD8                           (8)
#define SDHC_CMD9                           (9)
#define SDHC_CMD10                          (10)
#define SDHC_CMD11                          (11)
#define SDHC_CMD12                          (12)
#define SDHC_CMD13                          (13)
#define SDHC_CMD15                          (15)
#define SDHC_CMD16                          (16)
#define SDHC_CMD17                          (17)
#define SDHC_CMD18                          (18)
#define SDHC_CMD20                          (20)
#define SDHC_CMD24                          (24)
#define SDHC_CMD25                          (25)
#define SDHC_CMD26                          (26)
#define SDHC_CMD27                          (27)
#define SDHC_CMD28                          (28)
#define SDHC_CMD29                          (29)
#define SDHC_CMD30                          (30)
#define SDHC_CMD32                          (32)
#define SDHC_CMD33                          (33)
#define SDHC_CMD34                          (34)
#define SDHC_CMD35                          (35)
#define SDHC_CMD36                          (36)
#define SDHC_CMD37                          (37)
#define SDHC_CMD38                          (38)
#define SDHC_CMD39                          (39)
#define SDHC_CMD40                          (40)
#define SDHC_CMD42                          (42)
#define SDHC_CMD52                          (52)
#define SDHC_CMD53                          (53)
#define SDHC_CMD55                          (55)
#define SDHC_CMD56                          (56)
#define SDHC_CMD60                          (60)
#define SDHC_CMD61                          (61)
#define SDHC_ACMD6                          (0x40 + 6)
#define SDHC_ACMD13                         (0x40 + 13)
#define SDHC_ACMD22                         (0x40 + 22)
#define SDHC_ACMD23                         (0x40 + 23)
#define SDHC_ACMD41                         (0x40 + 41)
#define SDHC_ACMD42                         (0x40 + 42)
#define SDHC_ACMD51                         (0x40 + 51)

#define SDHC_FIFO_BUFFER_SIZE               16
#define SDHC_BLOCK_SIZE                     512

/******************************************************************************
* Macros 
******************************************************************************/

// prescale can be 2, 4, 8, 16, 32, 64, 128, 256
// divisor can be 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
#define SDHC_SYSCTL_DIVISOR(prescale, divisor) \
	(SDHC_SYSCTL_SDCLKFS((prescale)>>1)|SDHC_SYSCTL_DVS((divisor)-1))

#if (F_CPU == 256000000)
  #define SDHC_SYSCTL_400KHZ  SDHC_SYSCTL_DIVISOR(64, 10) // 400 kHz
  #define SDHC_SYSCTL_25MHZ   SDHC_SYSCTL_DIVISOR(2, 6)   // 21.3 MHz
  #define SDHC_SYSCTL_50MHZ   SDHC_SYSCTL_DIVISOR(2, 3)   // 42.6 MHz
#elif (F_CPU == 240000000)
  #define SDHC_SYSCTL_400KHZ  SDHC_SYSCTL_DIVISOR(64, 10) // 375 kHz
  #define SDHC_SYSCTL_25MHZ   SDHC_SYSCTL_DIVISOR(2, 5)   // 24 MHz
  #define SDHC_SYSCTL_50MHZ   SDHC_SYSCTL_DIVISOR(2, 3)   // 40 MHz
#elif (F_CPU == 216000000)
  #define SDHC_SYSCTL_400KHZ  SDHC_SYSCTL_DIVISOR(64, 9)  // 375 kHz
  #define SDHC_SYSCTL_25MHZ   SDHC_SYSCTL_DIVISOR(2, 5)   // 21.6 MHz
  #define SDHC_SYSCTL_50MHZ   SDHC_SYSCTL_DIVISOR(2, 3)   // 36 MHz
#elif (F_CPU == 192000000)
  #define SDHC_SYSCTL_400KHZ  SDHC_SYSCTL_DIVISOR(32, 15) // 400 kHz
  #define SDHC_SYSCTL_25MHZ   SDHC_SYSCTL_DIVISOR(2, 4)   // 24 MHz
  #define SDHC_SYSCTL_50MHZ   SDHC_SYSCTL_DIVISOR(2, 2)   // 48 MHz
#elif (F_CPU == 180000000)
  #define SDHC_SYSCTL_400KHZ  SDHC_SYSCTL_DIVISOR(32, 15) // 351 kHz
  #define SDHC_SYSCTL_25MHZ   SDHC_SYSCTL_DIVISOR(2, 4)   // 22.5 MHz
  #define SDHC_SYSCTL_50MHZ   SDHC_SYSCTL_DIVISOR(2, 2)   // 45 MHz
#elif (F_CPU == 168000000)
  #define SDHC_SYSCTL_400KHZ  SDHC_SYSCTL_DIVISOR(32, 14) // 375 kHz
  #define SDHC_SYSCTL_25MHZ   SDHC_SYSCTL_DIVISOR(2, 4)   // 21 MHz
  #define SDHC_SYSCTL_50MHZ   SDHC_SYSCTL_DIVISOR(2, 2)   // 42 MHz
#elif (F_CPU == 144000000)
  #define SDHC_SYSCTL_400KHZ  SDHC_SYSCTL_DIVISOR(32, 12) // 375 kHz
  #define SDHC_SYSCTL_25MHZ   SDHC_SYSCTL_DIVISOR(2, 3)   // 24 MHz
  #define SDHC_SYSCTL_50MHZ   SDHC_SYSCTL_DIVISOR(2, 2)   // 36 MHz
#elif (F_CPU == 120000000)
  #define SDHC_SYSCTL_400KHZ  SDHC_SYSCTL_DIVISOR(32, 10) // 375 kHz
  #define SDHC_SYSCTL_25MHZ   SDHC_SYSCTL_DIVISOR(2, 3)   // 20 MHz
  #define SDHC_SYSCTL_50MHZ   SDHC_SYSCTL_DIVISOR(2, 2)   // 30 MHz
#elif (F_CPU == 96000000)
  #define SDHC_SYSCTL_400KHZ  SDHC_SYSCTL_DIVISOR(16, 15) // 400 kHz
  #define SDHC_SYSCTL_25MHZ   SDHC_SYSCTL_DIVISOR(2, 2)   // 24 MHz
  #define SDHC_SYSCTL_50MHZ   SDHC_SYSCTL_DIVISOR(2, 1)   // 48 MHz
#elif (F_CPU == 72000000)
  #define SDHC_SYSCTL_400KHZ  SDHC_SYSCTL_DIVISOR(16, 12) // 375 kHz
  #define SDHC_SYSCTL_25MHZ   SDHC_SYSCTL_DIVISOR(2, 2)   // 18 MHz
  #define SDHC_SYSCTL_50MHZ   SDHC_SYSCTL_DIVISOR(2, 1)   // 36 MHz
#elif (F_CPU == 48000000)
  #define SDHC_SYSCTL_400KHZ  SDHC_SYSCTL_DIVISOR(8, 15)  // 400 kHz
  #define SDHC_SYSCTL_25MHZ   SDHC_SYSCTL_DIVISOR(2, 1)   // 24 MHz
  #define SDHC_SYSCTL_50MHZ   SDHC_SYSCTL_DIVISOR(2, 1)   // 24 MHz
#elif (F_CPU == 24000000)
  #define SDHC_SYSCTL_400KHZ  SDHC_SYSCTL_DIVISOR(4, 15)  // 500 kHz
  #define SDHC_SYSCTL_25MHZ   SDHC_SYSCTL_DIVISOR(2, 1)   // 12 MHz
  #define SDHC_SYSCTL_50MHZ   SDHC_SYSCTL_DIVISOR(2, 1)   // 12 MHz
#elif (F_CPU == 16000000)
  #define SDHC_SYSCTL_400KHZ  SDHC_SYSCTL_DIVISOR(4, 10)  // 400 kHz
  #define SDHC_SYSCTL_25MHZ   SDHC_SYSCTL_DIVISOR(2, 1)   // 8 MHz
  #define SDHC_SYSCTL_50MHZ   SDHC_SYSCTL_DIVISOR(2, 1)   // 8 MHz
#elif (F_CPU == 8000000)
  #define SDHC_SYSCTL_400KHZ  SDHC_SYSCTL_DIVISOR(2, 10)  // 400 kHz
  #define SDHC_SYSCTL_25MHZ   SDHC_SYSCTL_DIVISOR(2, 1)   // 4 MHz
  #define SDHC_SYSCTL_50MHZ   SDHC_SYSCTL_DIVISOR(2, 1)   // 4 MHz
#elif (F_CPU == 4000000)
  #define SDHC_SYSCTL_400KHZ  SDHC_SYSCTL_DIVISOR(2, 5)   // 400 kHz
  #define SDHC_SYSCTL_25MHZ   SDHC_SYSCTL_DIVISOR(2, 1)   // 2 MHz
  #define SDHC_SYSCTL_50MHZ   SDHC_SYSCTL_DIVISOR(2, 1)   // 2 MHz
#elif (F_CPU == 2000000)
  #define SDHC_SYSCTL_400KHZ  SDHC_SYSCTL_DIVISOR(2, 3)   // 333 kHz
  #define SDHC_SYSCTL_25MHZ   SDHC_SYSCTL_DIVISOR(2, 1)   // 1 MHz
  #define SDHC_SYSCTL_50MHZ   SDHC_SYSCTL_DIVISOR(2, 1)   // 1 MHz
#endif


/******************************************************************************
* Types
******************************************************************************/
typedef struct {
	uint8_t  status;
	uint8_t  highCapacity;  
	uint8_t  version2;
	uint8_t  tranSpeed;
	uint32_t address;
	uint32_t numBlocks;
	uint32_t lastCardStatus;
} SD_CARD_DESCRIPTOR;

/******************************************************************************
* Global functions
******************************************************************************/

uint8_t SDHC_InitCard(void);
uint8_t SDHC_GetStatus(void);
uint32_t SDHC_GetBlockCnt(void);

int SDHC_ReadBlocks(void * buff, uint32_t sector);
int SDHC_WriteBlocks(const void * buff, uint32_t sector);




/******************************************************************************
* Private variables
******************************************************************************/

static SD_CARD_DESCRIPTOR sdCardDesc;

/******************************************************************************
* Private functions
******************************************************************************/

static uint8_t SDHC_Init(void);
static void SDHC_InitGPIO(void);
static void SDHC_ReleaseGPIO(void);
static void SDHC_SetClock(uint32_t sysctl);
static uint32_t SDHC_WaitStatus(uint32_t mask);
static int SDHC_ReadBlock(uint32_t* pData);
static int SDHC_WriteBlock(const uint32_t* pData);
static int SDHC_CMD_Do(uint32_t xfertyp);
static int SDHC_CMD0_GoToIdle(void);
static int SDHC_CMD2_Identify(void);
static int SDHC_CMD3_GetAddress(void);
static int SDHC_ACMD6_SetBusWidth(uint32_t address, uint32_t width);
static int SDHC_CMD7_SelectCard(uint32_t address);
static int SDHC_CMD8_SetInterface(uint32_t cond);
static int SDHC_CMD9_GetParameters(uint32_t address);
static int SDHC_CMD12_StopTransfer(void);
static int SDHC_CMD12_StopTransferWaitForBusy(void);
static int SDHC_CMD16_SetBlockSize(uint32_t block_size);
static int SDHC_CMD17_ReadBlock(uint32_t sector);
static int SDHC_CMD24_WriteBlock(uint32_t sector);
static int SDHC_ACMD41_SendOperationCond(uint32_t cond);


/******************************************************************************
*
*   Public functions
*
******************************************************************************/


// initialize the SDHC Controller
// returns status of initialization(OK, nonInit, noCard, CardProtected)
static uint8_t SDHC_Init(void)
{
	int i;

	// Enable clock to SDHC peripheral
	SIM_SCGC3 |= SIM_SCGC3_SDHC;

	// Enable clock to PORT E peripheral (all SDHC BUS signals)
	SIM_SCGC5 |= SIM_SCGC5_PORTE;

	SIM_SCGC6 |= SIM_SCGC6_DMAMUX;
	SIM_SCGC7 |= SIM_SCGC7_DMA;

	// Enable DMA access via MPU (not currently used)
	MPU_CESR &= ~MPU_CESR_VLD_MASK;

	// De-init GPIO - to prevent unwanted clocks on bus
	SDHC_ReleaseGPIO();

	/* Reset SDHC */
	SDHC_SYSCTL = SDHC_SYSCTL_RSTA | SDHC_SYSCTL_SDCLKFS(0x80);
	while (SDHC_SYSCTL & SDHC_SYSCTL_RSTA) ; // wait

	/* Initial values */ // to do - Check values
	SDHC_VENDOR = 0;
	SDHC_BLKATTR = SDHC_BLKATTR_BLKCNT(1) | SDHC_BLKATTR_BLKSIZE(SDHC_BLOCK_SIZE);
	SDHC_PROCTL = SDHC_PROCTL_EMODE(SDHC_PROCTL_EMODE_INVARIANT) | SDHC_PROCTL_D3CD;
	SDHC_WML = SDHC_WML_RDWML(SDHC_FIFO_BUFFER_SIZE) | SDHC_WML_WRWML(SDHC_FIFO_BUFFER_SIZE);

	/* Set the SDHC initial baud rate divider and start */
	//SDHC_SetBaudrate(400);
	SDHC_SetClock(SDHC_SYSCTL_400KHZ);

	/* Poll inhibit bits */
	while (SDHC_PRSSTAT & (SDHC_PRSSTAT_CIHB | SDHC_PRSSTAT_CDIHB)) { };

	/* Init GPIO again */
	SDHC_InitGPIO();

	/* Enable requests */
	SDHC_IRQSTAT = 0xFFFF;
	SDHC_IRQSTATEN = SDHC_IRQSTATEN_DMAESEN | SDHC_IRQSTATEN_AC12ESEN | SDHC_IRQSTATEN_DEBESEN |
		SDHC_IRQSTATEN_DCESEN | SDHC_IRQSTATEN_DTOESEN | SDHC_IRQSTATEN_CIESEN |
		SDHC_IRQSTATEN_CEBESEN | SDHC_IRQSTATEN_CCESEN | SDHC_IRQSTATEN_CTOESEN |
		SDHC_IRQSTATEN_BRRSEN | SDHC_IRQSTATEN_BWRSEN | SDHC_IRQSTATEN_DINTSEN |
		SDHC_IRQSTATEN_CRMSEN | SDHC_IRQSTATEN_TCSEN | SDHC_IRQSTATEN_CCSEN;

	// initial clocks... SD spec says only 74 clocks are needed, but if Teensy rebooted
	// while the card was in middle of an operation, thousands of clock cycles can be
	// needed to get the card to complete a prior command and return to a usable state.
	for (i=0; i < 500; i++) {
		SDHC_SYSCTL |= SDHC_SYSCTL_INITA;
		while (SDHC_SYSCTL & SDHC_SYSCTL_INITA) { };
	}

	// to do - check if this needed
	SDHC_IRQSTAT |= SDHC_IRQSTAT_CRM;

	// Check card
	if (SDHC_PRSSTAT & SDHC_PRSSTAT_CINS) {
		return 0;
	} else {
		return SDHC_STATUS_NODISK;
	}
}

uint8_t KinetisSDHC_GetCardType(void)
{
	if (sdCardDesc.status) return 0;
	if (sdCardDesc.version2 == 0) return 1; // SD_CARD_TYPE_SD1
	if (sdCardDesc.highCapacity == 0) return 2; // SD_CARD_TYPE_SD2
	return 3; // SD_CARD_TYPE_SDHC
}

// initialize the SDHC Controller and SD Card
// returns status of initialization(OK, nonInit, noCard, CardProtected)
uint8_t KinetisSDHC_InitCard(void)
{
  uint8_t resS;
  int resR;
  uint32_t i;

  resS = SDHC_Init();

  sdCardDesc.status = resS;
  sdCardDesc.address = 0;
  sdCardDesc.highCapacity = 0;
  sdCardDesc.version2 = 0;
  sdCardDesc.numBlocks = 0;

  if (resS)
    return resS;

  resR = SDHC_CMD0_GoToIdle();
  if (resR) {
    sdCardDesc.status = SDHC_STATUS_NOINIT;
    return SDHC_STATUS_NOINIT;
  }

  resR = SDHC_CMD8_SetInterface(0x000001AA); // 3.3V and AA check pattern
  if (resR == SDHC_RESULT_OK) {
      if (SDHC_CMDRSP0 != 0x000001AA) {
        sdCardDesc.status = SDHC_STATUS_NOINIT;
        return SDHC_STATUS_NOINIT;
      }
      sdCardDesc.highCapacity = 1;
  } else if (resR == SDHC_RESULT_NO_RESPONSE) {
      // version 1 cards do not respond to CMD8
  } else {
    sdCardDesc.status = SDHC_STATUS_NOINIT;
    return SDHC_STATUS_NOINIT;
  }

  if (SDHC_ACMD41_SendOperationCond(0)) {
    sdCardDesc.status = SDHC_STATUS_NOINIT;
    return SDHC_STATUS_NOINIT;
  }

  if (SDHC_CMDRSP0 & 0x300000) {
    uint32_t condition = 0x00300000;
    if (sdCardDesc.highCapacity)
            condition |= 0x40000000;
    i = 0;
    do {
        i++;
        if (SDHC_ACMD41_SendOperationCond(condition)) {
          resS = SDHC_STATUS_NOINIT;
          break;
        }
    } while ((!(SDHC_CMDRSP0 & 0x80000000)) && (i < SDHC_INITIALIZATION_MAX_CNT));


    if (resS)
      return resS;

    if ((i >= SDHC_INITIALIZATION_MAX_CNT) || (!(SDHC_CMDRSP0 & 0x40000000)))
        sdCardDesc.highCapacity = 0;
  }

  // Card identify
  if(SDHC_CMD2_Identify()) {
    sdCardDesc.status = SDHC_STATUS_NOINIT;
    return SDHC_STATUS_NOINIT;
  }

  // Get card address
  if(SDHC_CMD3_GetAddress()) {
    sdCardDesc.status = SDHC_STATUS_NOINIT;
    return SDHC_STATUS_NOINIT;
  }

  sdCardDesc.address = SDHC_CMDRSP0 & 0xFFFF0000;

  // Get card parameters
  if(SDHC_CMD9_GetParameters(sdCardDesc.address)) {
    sdCardDesc.status = SDHC_STATUS_NOINIT;
    return SDHC_STATUS_NOINIT;
  }

  if (!(SDHC_CMDRSP3 & 0x00C00000)) {
    uint32_t read_bl_len, c_size, c_size_mult;

    read_bl_len = (SDHC_CMDRSP2 >> 8) & 0x0F;
    c_size = SDHC_CMDRSP2 & 0x03;
    c_size = (c_size << 10) | (SDHC_CMDRSP1 >> 22);
    c_size_mult = (SDHC_CMDRSP1 >> 7) & 0x07;
    sdCardDesc.numBlocks = (c_size + 1) * (1 << (c_size_mult + 2)) * (1 << (read_bl_len - 9));
  } else {
    uint32_t c_size;
    sdCardDesc.version2 = 1;
    c_size = (SDHC_CMDRSP1 >> 8) & 0x003FFFFF;
    sdCardDesc.numBlocks = (c_size + 1) << 10;
  }

  // Select card
  if (SDHC_CMD7_SelectCard(sdCardDesc.address)) {
    sdCardDesc.status = SDHC_STATUS_NOINIT;
    return SDHC_STATUS_NOINIT;
  }

  // Set Block Size to 512
  // Block Size in SDHC Controller is already set to 512 by SDHC_Init();
  // Set 512 Block size in SD card
  if (SDHC_CMD16_SetBlockSize(SDHC_BLOCK_SIZE)) {
    sdCardDesc.status = SDHC_STATUS_NOINIT;
    return SDHC_STATUS_NOINIT;
  }

  // Set 4 bit data bus width
  if (SDHC_ACMD6_SetBusWidth(sdCardDesc.address, 2)) {
    sdCardDesc.status = SDHC_STATUS_NOINIT;
    return SDHC_STATUS_NOINIT;
  }

  // Set Data bus width also in SDHC controller
  SDHC_PROCTL &= (~ SDHC_PROCTL_DTW_MASK);
  SDHC_PROCTL |= SDHC_PROCTL_DTW(SDHC_PROCTL_DTW_4BIT);

  // De-Init GPIO
  SDHC_ReleaseGPIO();

  // Set the SDHC default baud rate
  SDHC_SetClock(SDHC_SYSCTL_25MHZ);
  // TODO: use CMD6 and CMD9 to detect if card supports 50 MHz
  // then use CMD4 to configure card to high speed mode,
  // and SDHC_SetClock() for 50 MHz config

  // Init GPIO
  SDHC_InitGPIO();

  return sdCardDesc.status;
}


// read a block from disk
//   buff - pointer on buffer where read data should be stored
//   sector - index of start sector
int KinetisSDHC_ReadBlock(void * buff, uint32_t sector)
{
  int result;
  uint32_t* pData = (uint32_t*)buff;

  // Check if this is ready
  if (sdCardDesc.status != 0)
     return SDHC_RESULT_NOT_READY;

  // Convert LBA to uint8_t address if needed
  if (!sdCardDesc.highCapacity)
    sector *= 512;

  SDHC_IRQSTAT = 0xffff;

  // Just single block mode is needed
  result = SDHC_CMD17_ReadBlock(sector);
  if(result != SDHC_RESULT_OK) return result;
  result = SDHC_ReadBlock(pData);

  // finish up
  while (!(SDHC_IRQSTAT & SDHC_IRQSTAT_TC)) { }  // wait for transfer to complete
  SDHC_IRQSTAT = (SDHC_IRQSTAT_TC | SDHC_IRQSTAT_BRR | SDHC_IRQSTAT_AC12E);

  return result;
}

//-----------------------------------------------------------------------------
// FUNCTION:    disk_write
// SCOPE:       SDHC public related function
// DESCRIPTION: Function write block/blocks to disk
//
// PARAMETERS:  buff - pointer on buffer where is stored data
//              sector - index of start sector
//              count - count of sector to write
//
// RETURNS:     result of operation
//-----------------------------------------------------------------------------
int KinetisSDHC_WriteBlock(const void * buff, uint32_t sector)
{
  int result;
  const uint32_t *pData = (const uint32_t *)buff;

  // Check if this is ready
  if (sdCardDesc.status != 0) return SDHC_RESULT_NOT_READY;

  // Convert LBA to uint8_t address if needed
  if(!sdCardDesc.highCapacity)
    sector *= 512;

  SDHC_IRQSTAT = 0xffff;

  // Just single block mode is needed
  result = SDHC_CMD24_WriteBlock(sector);
  if (result != SDHC_RESULT_OK) return result;
  result = SDHC_WriteBlock(pData);

  // finish up
  while (!(SDHC_IRQSTAT & SDHC_IRQSTAT_TC)) { }  // wait for transfer to complete
  SDHC_IRQSTAT = (SDHC_IRQSTAT_TC | SDHC_IRQSTAT_BWR | SDHC_IRQSTAT_AC12E);

  return result;
}

/******************************************************************************
*
*   Private functions
*
******************************************************************************/

// initialize the SDHC Controller signals
static void SDHC_InitGPIO(void)
{
  PORTE_PCR0 = PORT_PCR_MUX(4) | PORT_PCR_PS | PORT_PCR_PE | PORT_PCR_DSE; /* SDHC.D1  */
  PORTE_PCR1 = PORT_PCR_MUX(4) | PORT_PCR_PS | PORT_PCR_PE | PORT_PCR_DSE; /* SDHC.D0  */
  PORTE_PCR2 = PORT_PCR_MUX(4) | PORT_PCR_DSE;                             /* SDHC.CLK */
  PORTE_PCR3 = PORT_PCR_MUX(4) | PORT_PCR_PS | PORT_PCR_PE | PORT_PCR_DSE; /* SDHC.CMD */
  PORTE_PCR4 = PORT_PCR_MUX(4) | PORT_PCR_PS | PORT_PCR_PE | PORT_PCR_DSE; /* SDHC.D3  */
  PORTE_PCR5 = PORT_PCR_MUX(4) | PORT_PCR_PS | PORT_PCR_PE | PORT_PCR_DSE; /* SDHC.D2  */
}

// release the SDHC Controller signals
static void SDHC_ReleaseGPIO(void)
{
  PORTE_PCR0 = PORT_PCR_MUX(1) | PORT_PCR_PE | PORT_PCR_PS; 	/* PULLUP SDHC.D1  */
  PORTE_PCR1 = PORT_PCR_MUX(1) | PORT_PCR_PE | PORT_PCR_PS; 	/* PULLUP SDHC.D0  */
  PORTE_PCR2 = 0;						/* SDHC.CLK */
  PORTE_PCR3 = PORT_PCR_MUX(1) | PORT_PCR_PE | PORT_PCR_PS; 	/* PULLUP SDHC.CMD */
  PORTE_PCR4 = PORT_PCR_MUX(1) | PORT_PCR_PE | PORT_PCR_PS;	/* PULLUP SDHC.D3  */
  PORTE_PCR5 = PORT_PCR_MUX(1) | PORT_PCR_PE | PORT_PCR_PS; 	/* PULLUP SDHC.D2  */
}



static void SDHC_SetClock(uint32_t sysctl)
{
	uint32_t n, timeout;

	n = SDHC_SYSCTL;
	// Disable SDHC clocks
	n &= ~SDHC_SYSCTL_SDCLKEN;
	SDHC_SYSCTL = n;
	// Change dividers
	n &= ~(SDHC_SYSCTL_DTOCV_MASK | SDHC_SYSCTL_SDCLKFS_MASK | SDHC_SYSCTL_DVS_MASK);
	n |= sysctl | SDHC_SYSCTL_DTOCV(0x0E);
	SDHC_SYSCTL = n;
	/* Wait for stable clock */
	timeout = 0xFFFFF;
	while ((!(SDHC_PRSSTAT & SDHC_PRSSTAT_SDSTB)) && timeout) {
		timeout--;
	}
	/* Enable SDHC clocks */
	SDHC_SYSCTL = n | SDHC_SYSCTL_SDCLKEN;
	SDHC_IRQSTAT |= SDHC_IRQSTAT_DTOE;
}

// waits for status bits sets
static uint32_t SDHC_WaitStatus(uint32_t mask)
{
    uint32_t             result;
    uint32_t             timeout = 1<<24;
    do {
        result = SDHC_IRQSTAT & mask;
        timeout--;
    }
    while(!result && (timeout));

    if (timeout)
      return result;

    return 0;
}

// reads one block
static int SDHC_ReadBlock(uint32_t* pData)
{
	uint32_t i, irqstat;
	const uint32_t i_max = ((SDHC_BLOCK_SIZE) / (4 * SDHC_FIFO_BUFFER_SIZE));

	for (i = 0; i < i_max; i++) {
		irqstat = SDHC_IRQSTAT;
		SDHC_IRQSTAT = irqstat | SDHC_IRQSTAT_BRR;
		if (irqstat & (SDHC_IRQSTAT_DEBE | SDHC_IRQSTAT_DCE | SDHC_IRQSTAT_DTOE)) {
			SDHC_IRQSTAT = irqstat | SDHC_IRQSTAT_BRR | 
				SDHC_IRQSTAT_DEBE | SDHC_IRQSTAT_DCE | SDHC_IRQSTAT_DTOE;
			SDHC_CMD12_StopTransferWaitForBusy();
			return SDHC_RESULT_ERROR;
		}
		while (!(SDHC_PRSSTAT & SDHC_PRSSTAT_BREN)) { };
		*pData++ = SDHC_DATPORT;
		*pData++ = SDHC_DATPORT;
		*pData++ = SDHC_DATPORT;
		*pData++ = SDHC_DATPORT;
		*pData++ = SDHC_DATPORT;
		*pData++ = SDHC_DATPORT;
		*pData++ = SDHC_DATPORT;
		*pData++ = SDHC_DATPORT;
		*pData++ = SDHC_DATPORT;
		*pData++ = SDHC_DATPORT;
		*pData++ = SDHC_DATPORT;
		*pData++ = SDHC_DATPORT;
		*pData++ = SDHC_DATPORT;
		*pData++ = SDHC_DATPORT;
		*pData++ = SDHC_DATPORT;
		*pData++ = SDHC_DATPORT;
	}
	return SDHC_RESULT_OK;
}

// writes one block
static int SDHC_WriteBlock(const uint32_t* pData)
{
	uint32_t i, i_max, j;
	i_max = ((SDHC_BLOCK_SIZE) / (4 * SDHC_FIFO_BUFFER_SIZE));

	for(i = 0; i < i_max; i++) {
		while (!(SDHC_IRQSTAT & SDHC_IRQSTAT_BWR)) ; // wait
		if (SDHC_IRQSTAT & (SDHC_IRQSTAT_DEBE | SDHC_IRQSTAT_DCE | SDHC_IRQSTAT_DTOE)) {
        		SDHC_IRQSTAT |= SDHC_IRQSTAT_DEBE | SDHC_IRQSTAT_DCE |
				SDHC_IRQSTAT_DTOE | SDHC_IRQSTAT_BWR;
        		(void)SDHC_CMD12_StopTransferWaitForBusy();
			return SDHC_RESULT_ERROR;
		}
		for(j=0; j<SDHC_FIFO_BUFFER_SIZE; j++) {
			SDHC_DATPORT = *pData++;
		}
		SDHC_IRQSTAT |= SDHC_IRQSTAT_BWR;

		if (SDHC_IRQSTAT & (SDHC_IRQSTAT_DEBE | SDHC_IRQSTAT_DCE | SDHC_IRQSTAT_DTOE)) {
			SDHC_IRQSTAT |= SDHC_IRQSTAT_DEBE | SDHC_IRQSTAT_DCE |
				SDHC_IRQSTAT_DTOE | SDHC_IRQSTAT_BWR;
			(void)SDHC_CMD12_StopTransferWaitForBusy();
			return SDHC_RESULT_ERROR;
		}
	}
	return SDHC_RESULT_OK;
}

// sends the command to SDcard
static int SDHC_CMD_Do(uint32_t xfertyp)
{

    // Card removal check preparation
    SDHC_IRQSTAT |= SDHC_IRQSTAT_CRM;

    // Wait for cmd line idle // to do timeout PRSSTAT[CDIHB] and the PRSSTAT[CIHB]
    while ((SDHC_PRSSTAT & SDHC_PRSSTAT_CIHB) || (SDHC_PRSSTAT & SDHC_PRSSTAT_CDIHB))
        { };

    SDHC_XFERTYP = xfertyp;

    /* Wait for response */
    if (SDHC_WaitStatus(SDHC_IRQSTAT_CIE | SDHC_IRQSTAT_CEBE | SDHC_IRQSTAT_CCE | SDHC_IRQSTAT_CC) != SDHC_IRQSTAT_CC) {
	SDHC_IRQSTAT |= SDHC_IRQSTAT_CTOE | SDHC_IRQSTAT_CIE | SDHC_IRQSTAT_CEBE |
						SDHC_IRQSTAT_CCE | SDHC_IRQSTAT_CC;
        return SDHC_RESULT_ERROR;
    }

    /* Check card removal */
    if (SDHC_IRQSTAT & SDHC_IRQSTAT_CRM) {
        SDHC_IRQSTAT |= SDHC_IRQSTAT_CTOE | SDHC_IRQSTAT_CC;
        return SDHC_RESULT_NOT_READY;
    }

    /* Get response, if available */
    if (SDHC_IRQSTAT & SDHC_IRQSTAT_CTOE) {
        SDHC_IRQSTAT |= SDHC_IRQSTAT_CTOE | SDHC_IRQSTAT_CC;
        return SDHC_RESULT_NO_RESPONSE;
    }

    SDHC_IRQSTAT |= SDHC_IRQSTAT_CC;

    return SDHC_RESULT_OK;

}

// sends CMD0 to put SDCARD to idle
static int SDHC_CMD0_GoToIdle(void)
{
  uint32_t xfertyp;
  int result;

  SDHC_CMDARG = 0;

  xfertyp = (SDHC_XFERTYP_CMDINX(SDHC_CMD0) | SDHC_XFERTYP_RSPTYP(SDHC_XFERTYP_RSPTYP_NO));

  result = SDHC_CMD_Do(xfertyp);

  if(result == SDHC_RESULT_OK) {
        (void)SDHC_CMDRSP0;
  }

  return result;
}

// sends CMD2 to identify card
static int SDHC_CMD2_Identify(void)
{
  uint32_t xfertyp;
  int result;

  SDHC_CMDARG = 0;

  xfertyp = (SDHC_XFERTYP_CMDINX(SDHC_CMD2) | SDHC_XFERTYP_CCCEN |
             SDHC_XFERTYP_RSPTYP(SDHC_XFERTYP_RSPTYP_136));

  result = SDHC_CMD_Do(xfertyp);

  if(result == SDHC_RESULT_OK) {
        (void)SDHC_CMDRSP0;
  }

  return result;
}

// sends CMD 3 to get address
static int SDHC_CMD3_GetAddress(void)
{
  uint32_t xfertyp;
  int result;

  SDHC_CMDARG = 0;

  xfertyp = (SDHC_XFERTYP_CMDINX(SDHC_CMD3) | SDHC_XFERTYP_CICEN |
             SDHC_XFERTYP_CCCEN | SDHC_XFERTYP_RSPTYP(SDHC_XFERTYP_RSPTYP_48));

  result = SDHC_CMD_Do(xfertyp);

  if (result == SDHC_RESULT_OK) {
        (void)SDHC_CMDRSP0;
  }
  return result;
}

// sends ACMD6 to set bus width
static int SDHC_ACMD6_SetBusWidth(uint32_t address, uint32_t width)
{
  uint32_t xfertyp;
  int result;

  SDHC_CMDARG = address;
  // first send CMD 55 Application specific command
  xfertyp = (SDHC_XFERTYP_CMDINX(SDHC_CMD55) | SDHC_XFERTYP_CICEN |
             SDHC_XFERTYP_CCCEN | SDHC_XFERTYP_RSPTYP(SDHC_XFERTYP_RSPTYP_48));

  result = SDHC_CMD_Do(xfertyp);

  if(result == SDHC_RESULT_OK) {
    (void)SDHC_CMDRSP0;
  } else {
    return result;
  }
  SDHC_CMDARG = width;

  // Send 6CMD
  xfertyp = (SDHC_XFERTYP_CMDINX(SDHC_CMD6) | SDHC_XFERTYP_CICEN |
             SDHC_XFERTYP_CCCEN | SDHC_XFERTYP_RSPTYP(SDHC_XFERTYP_RSPTYP_48));

  result = SDHC_CMD_Do(xfertyp);

  if(result == SDHC_RESULT_OK) {
        (void)SDHC_CMDRSP0;
  }
  return result;
}


// sends CMD 7 to select card
static int SDHC_CMD7_SelectCard(uint32_t address)
{
  uint32_t xfertyp;
  int result;

  SDHC_CMDARG = address;

  xfertyp = (SDHC_XFERTYP_CMDINX(SDHC_CMD7) | SDHC_XFERTYP_CICEN |
             SDHC_XFERTYP_CCCEN | SDHC_XFERTYP_RSPTYP(SDHC_XFERTYP_RSPTYP_48BUSY));

  result = SDHC_CMD_Do(xfertyp);

  if(result == SDHC_RESULT_OK) {
        (void)SDHC_CMDRSP0;
  }
  return result;
}

// CMD8 to send interface condition
static int SDHC_CMD8_SetInterface(uint32_t cond)
{
  uint32_t xfertyp;
  int result;

  SDHC_CMDARG = cond;

  xfertyp = (SDHC_XFERTYP_CMDINX(SDHC_CMD8) | SDHC_XFERTYP_CICEN |
             SDHC_XFERTYP_CCCEN | SDHC_XFERTYP_RSPTYP(SDHC_XFERTYP_RSPTYP_48));

  result = SDHC_CMD_Do(xfertyp);

  if(result == SDHC_RESULT_OK) {
        (void)SDHC_CMDRSP0;
  }
  return result;
}

// sends CMD 8 to send interface condition
static int SDHC_CMD9_GetParameters(uint32_t address)
{
  uint32_t xfertyp;
  int result;

  SDHC_CMDARG = address;

  xfertyp = (SDHC_XFERTYP_CMDINX(SDHC_CMD9) | SDHC_XFERTYP_CCCEN |
             SDHC_XFERTYP_RSPTYP(SDHC_XFERTYP_RSPTYP_136));

  result = SDHC_CMD_Do(xfertyp);

  if (result == SDHC_RESULT_OK) {
        //(void)SDHC_CMDRSP0;
	sdCardDesc.tranSpeed = SDHC_CMDRSP2 >> 24;
  }

  return result;
}


// sends CMD12 to stop transfer
static int SDHC_CMD12_StopTransfer(void)
{
  uint32_t xfertyp;
  int result;

  SDHC_CMDARG = 0;
  xfertyp = (SDHC_XFERTYP_CMDINX(SDHC_CMD12) | SDHC_XFERTYP_CMDTYP(SDHC_XFERTYP_CMDTYP_ABORT) |
                 SDHC_XFERTYP_CICEN | SDHC_XFERTYP_CCCEN | SDHC_XFERTYP_RSPTYP(SDHC_XFERTYP_RSPTYP_48BUSY));

  result = SDHC_CMD_Do(xfertyp);

  if (result == SDHC_RESULT_OK) {
  }
  return result;
}

// sends CMD12 to stop transfer and first waits to ready SDCArd
static int SDHC_CMD12_StopTransferWaitForBusy(void)
{
  uint32_t timeOut = 100;
  int result;
  do {
    result = SDHC_CMD12_StopTransfer();
    timeOut--;
  } while(timeOut && (SDHC_PRSSTAT & SDHC_PRSSTAT_DLA) && result == SDHC_RESULT_OK);

  if (result != SDHC_RESULT_OK)
    return result;

  if(!timeOut)
    return SDHC_RESULT_NO_RESPONSE;

  return SDHC_RESULT_OK;
}

// sends CMD8 to set block size
static int SDHC_CMD16_SetBlockSize(uint32_t block_size)
{
  uint32_t xfertyp;
  int result;

  SDHC_CMDARG = block_size;

  xfertyp = (SDHC_XFERTYP_CMDINX(SDHC_CMD16) | SDHC_XFERTYP_CICEN |
             SDHC_XFERTYP_CCCEN | SDHC_XFERTYP_RSPTYP(SDHC_XFERTYP_RSPTYP_48));

  result = SDHC_CMD_Do(xfertyp);

  if (result == SDHC_RESULT_OK) {
        (void)SDHC_CMDRSP0;
  }

  return result;
}

// sends CMD17 to read one block
static int SDHC_CMD17_ReadBlock(uint32_t sector)
{
  uint32_t xfertyp;
  int result;

  SDHC_CMDARG = sector;

  SDHC_BLKATTR = SDHC_BLKATTR_BLKCNT(1) | 512;

  xfertyp = (SDHC_XFERTYP_CMDINX(SDHC_CMD17) | SDHC_XFERTYP_CICEN |
                 SDHC_XFERTYP_CCCEN | SDHC_XFERTYP_RSPTYP(SDHC_XFERTYP_RSPTYP_48) |
                 SDHC_XFERTYP_DTDSEL | SDHC_XFERTYP_DPSEL);

  result = SDHC_CMD_Do(xfertyp);
  if (result == SDHC_RESULT_OK) {
	(void)SDHC_CMDRSP0;
  }

  return result;
}

// sends CMD24 to write one block
static int SDHC_CMD24_WriteBlock(uint32_t sector)
{
  uint32_t xfertyp;
  int result;

  SDHC_CMDARG = sector;
  SDHC_BLKATTR = SDHC_BLKATTR_BLKCNT(1) | 512;

  xfertyp = (SDHC_XFERTYP_CMDINX(SDHC_CMD24) | SDHC_XFERTYP_CICEN |
                 SDHC_XFERTYP_CCCEN | SDHC_XFERTYP_RSPTYP(SDHC_XFERTYP_RSPTYP_48) |
                 SDHC_XFERTYP_DPSEL);

  result = SDHC_CMD_Do(xfertyp);
  if (result == SDHC_RESULT_OK) {
	(void)SDHC_CMDRSP0;
  }

  return result;
}

// ACMD 41 to send operation condition
static int SDHC_ACMD41_SendOperationCond(uint32_t cond)
{
  uint32_t xfertyp;
  int result;

  SDHC_CMDARG = 0;
  // first send CMD 55 Application specific command
  xfertyp = (SDHC_XFERTYP_CMDINX(SDHC_CMD55) | SDHC_XFERTYP_CICEN |
             SDHC_XFERTYP_CCCEN | SDHC_XFERTYP_RSPTYP(SDHC_XFERTYP_RSPTYP_48));

  result = SDHC_CMD_Do(xfertyp);

  if (result == SDHC_RESULT_OK) {
        (void)SDHC_CMDRSP0;
  } else {
	return result;
  }

  SDHC_CMDARG = cond;

  // Send 41CMD
  xfertyp = (SDHC_XFERTYP_CMDINX(SDHC_ACMD41) | SDHC_XFERTYP_RSPTYP(SDHC_XFERTYP_RSPTYP_48));
  result = SDHC_CMD_Do(xfertyp);

  if (result == SDHC_RESULT_OK) {
        (void)SDHC_CMDRSP0;
  }

  return result;
}

#endif // __MK64FX512__ or __MK66FX1M0__
