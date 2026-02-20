//=================================================================================================
//                   E2803 EPD Driver for 31.5" Control Board (ESP32-C5)
//
// File Name : epd.cpp
// Based on : EL351TW1.c from E Ink sample code
// Version : 1.0
//=================================================================================================

#define __EPD_C__

#include <Arduino.h>
#include <SPI.h>
#include <stdio.h>
#include <string.h>
#include "globals.h"
#include "pindefine.h"
#include "status.h"

extern "C" {
#include "epd.h"
#include "pmic.h"
#include "comm.h"
}

// SPI CS pin mapping - using TCA9555 expander pins
// 8 chip selects: EXP1_SPI1_CS0-3 (pins 0-3) and EXP1_SPI2_CS0-3 (pins 4-7)
static const unsigned char spiCsPin[8] = {
    EXP1_SPI1_CS0, EXP1_SPI1_CS1, EXP1_SPI1_CS2, EXP1_SPI1_CS3,
    EXP1_SPI2_CS0, EXP1_SPI2_CS1, EXP1_SPI2_CS2, EXP1_SPI2_CS3
};

// EPD register configuration values (from E Ink sample code)
const unsigned char PSR_V[2] = {
    0xDB, 0x89
};
const unsigned char PWR_V[6] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
const unsigned char POF_V[1] = {
    0x00
};
const unsigned char DRF_V[1] = {
    0x01
};
const unsigned char CDI_V[1] = {
    0xF7
};
const unsigned char TCON_V[2] = {
    0x03, 0x03
};
const unsigned char TRES_V[4] = {
    0x03, 0x20, 0x02, 0xD0  // Resolution: 800x720
};
const unsigned char CMD66_V[6] = {
    0x49, 0x55, 0x13, 0x5D, 0x05, 0x10
};
const unsigned char VCOM_WOUT_EN_V[6] = {
    0x00, 0x00, 0x80, 0xFF, 0xCA, 0x1B
};
const unsigned char EN_BUF_V[1] = {
    0x00
};
const unsigned char TM_TCON_V[7] = {
    0x00, 0xF4, 0x81, 0x40, 0x00, 0x00, 0x00
};
const unsigned char CCSET_V[1] = {
    0x01
};
const unsigned char PWS_V[1] = {
    0x22
};
const unsigned char SPIM_V[1] = {
    0x00
};

// Image data buffer
unsigned char epdImageDataBuffer[EPD_IMAGE_DATA_BUFFER];

//================== GPIO Control Functions ====================================

/**
 * @brief Set EPD reset pin level
 */
static void epdResetPin(unsigned int pinStatus)
{
    digitalWrite(EPD_RST, pinStatus);
}

/**
 * @brief Set all chip select pins to specified level
 * @param setLevel: GPIO_HIGH or GPIO_LOW
 */
void epdSetCsAll(unsigned int setLevel)
{
    // for(int i = 0; i < 8; i++)
    // {
    //     expander1.write1(spiCsPin[i], setLevel);
    // }
expander1.write1(EXP1_SPI1_CS0, setLevel);
expander1.write1(EXP1_SPI1_CS1, setLevel);
expander1.write1(EXP1_SPI1_CS2, setLevel);
expander1.write1(EXP1_SPI1_CS3, setLevel);
expander1.write1(EXP1_SPI2_CS0, setLevel);
expander1.write1(EXP1_SPI2_CS1, setLevel);
expander1.write1(EXP1_SPI2_CS2, setLevel);
expander1.write1(EXP1_SPI2_CS3, setLevel);

  expander1.pinMode1(EXP1_SPI1_CS3, OUTPUT);
  expander1.pinMode1(EXP1_SPI1_CS2, OUTPUT);
  expander1.pinMode1(EXP1_SPI1_CS1, OUTPUT);
  expander1.pinMode1(EXP1_SPI1_CS0, OUTPUT);
  expander1.pinMode1(EXP1_SPI2_CS3, OUTPUT);
  expander1.pinMode1(EXP1_SPI2_CS2, OUTPUT);
  expander1.pinMode1(EXP1_SPI2_CS1, OUTPUT);
  expander1.pinMode1(EXP1_SPI2_CS0, OUTPUT);

}

/**
 * @brief Set specific chip select pin
 * @param csNumber: CS index (0-7)
 * @param setLevel: GPIO_HIGH or GPIO_LOW
 */
void epdSetCs(unsigned char csNumber, unsigned int setLevel)
{
    if(csNumber < 8)
    {
        expander1.write1(spiCsPin[csNumber], setLevel);
        expander1.pinMode1(spiCsPin[csNumber], OUTPUT);
    }
}

/**
 * @brief Wait for BUSY pin to go HIGH
 */
void epdCheckBusyHigh(void)
{
    int timeout = 10000; // 10 second timeout
    int elapsed = 0;
    while(!(digitalRead(EPD_BUSY)))
    {
        delayms(1);
        elapsed++;
        if(elapsed >= timeout)
        {
            printf("ERROR: EPD BUSY timeout (waiting for HIGH)\n");
            return;
        }
    }
}

/**
 * @brief Wait for BUSY pin to go LOW
 */
void epdCheckBusyLow(void)
{
    int timeout = 10000; // 10 second timeout
    int elapsed = 0;
    while(digitalRead(EPD_BUSY))
    {
        delayms(1);
        elapsed++;
        if(elapsed >= timeout)
        {
            printf("ERROR: EPD BUSY timeout (waiting for LOW)\n");
            return;
        }
    }
}

//================== Hardware Reset ====================================

void epdHardwareReset(void)
{
    printf("EPD Hardware Reset...\n");
    epdResetPin(GPIO_LOW);
    delayms(20);
    epdResetPin(GPIO_HIGH);
    delayms(20);
    printf("EPD Hardware Reset complete\n");
}

//================== SPI Communication ====================================

/**
 * @brief Write command and data to EPD via SPI
 */
void epdWrite(unsigned char epdCommand, const unsigned char *epdData, unsigned int epdDataLength)
{
    spiTransmit(epdCommand, (unsigned char *)epdData, epdDataLength);
}

/**
 * @brief Read data from EPD via SPI
 */
void epdRead(unsigned char epdCommand, unsigned char *epdData, unsigned int epdDataLength)
{
    spiReceive(epdCommand, epdData, epdDataLength);
}

/**
 * @brief Write command only (no data) to EPD
 */
void epdWriteCommand(unsigned char epdCommand)
{
    spiTransmitCommand(epdCommand);
}

/**
 * @brief Write data only (after command) to EPD
 */
void epdWriteData(const unsigned char *epdData, unsigned int epdDataLength)
{
    spiTransmitData((unsigned char *)epdData, epdDataLength);
}

//================== EPD Initialization ====================================

void epdInit(void)
{
    printf("Initializing EPD...\n");

    // Hardware reset
    epdHardwareReset();
    epdCheckBusyHigh();

    // Initialize all driver ICs with same configuration
    epdSetCsAll(GPIO_LOW);
    epdWrite(EPD_CMD_VCOM_WOUT_EN, VCOM_WOUT_EN_V, sizeof(VCOM_WOUT_EN_V));
    epdSetCsAll(GPIO_HIGH);

    epdSetCsAll(GPIO_LOW);
    epdWrite(EPD_CMD_TM_TCON, TM_TCON_V, sizeof(TM_TCON_V));
    epdSetCsAll(GPIO_HIGH);

    epdSetCsAll(GPIO_LOW);
    epdWrite(EPD_CMD_CMD66, CMD66_V, sizeof(CMD66_V));
    epdSetCsAll(GPIO_HIGH);

    epdSetCsAll(GPIO_LOW);
    epdWrite(EPD_CMD_PSR, PSR_V, sizeof(PSR_V));
    epdSetCsAll(GPIO_HIGH);

    epdSetCsAll(GPIO_LOW);
    epdWrite(EPD_CMD_PWR, PWR_V, sizeof(PWR_V));
    epdSetCsAll(GPIO_HIGH);

    epdSetCsAll(GPIO_LOW);
    epdWrite(EPD_CMD_CDI, CDI_V, sizeof(CDI_V));
    epdSetCsAll(GPIO_HIGH);

    epdSetCsAll(GPIO_LOW);
    epdWrite(EPD_CMD_TCON, TCON_V, sizeof(TCON_V));
    epdSetCsAll(GPIO_HIGH);

    epdSetCsAll(GPIO_LOW);
    epdWrite(EPD_CMD_TRES, TRES_V, sizeof(TRES_V));
    epdSetCsAll(GPIO_HIGH);

    epdSetCsAll(GPIO_LOW);
    epdWrite(EPD_CMD_EN_BUF, EN_BUF_V, sizeof(EN_BUF_V));
    epdSetCsAll(GPIO_HIGH);

    epdSetCsAll(GPIO_LOW);
    epdWrite(EPD_CMD_PWS, PWS_V, sizeof(PWS_V));
    epdSetCsAll(GPIO_HIGH);

    epdSetCsAll(GPIO_LOW);
    epdWrite(EPD_CMD_SPIM, SPIM_V, sizeof(SPIM_V));
    epdSetCsAll(GPIO_HIGH);

    epdSetCsAll(GPIO_LOW);
    epdWrite(EPD_CMD_CCSET, CCSET_V, sizeof(CCSET_V));
    epdSetCsAll(GPIO_HIGH);

    printf("EPD initialization complete\n");
}

//================== Driver IC Status Check ====================================

unsigned char epdCheckDriverICStatus(void)
{
    unsigned char csx, status = DONE;
    unsigned char dataBuf[3];

    printf("Checking driver IC status...\n");

    for(csx = 0; csx < 8; csx++)
    {
        memset(dataBuf, 0, sizeof(dataBuf));
        epdSetCs(csx, GPIO_LOW);
        epdRead(0xF2, dataBuf, sizeof(dataBuf));
        epdSetCs(csx, GPIO_HIGH);

#if SHOW_LOG
        printf("Driver IC [%d] = 0x%02X 0x%02X 0x%02X\n", csx, dataBuf[0], dataBuf[1], dataBuf[2]);
#endif

        if((dataBuf[0] & 0x01) == 0x01)
        {
#if SHOW_LOG
            printf("Driver IC [%d] is ready.\n", csx);
#endif
        }
        else
        {
#if SHOW_LOG
            printf("Driver IC [%d] did not reply.\n", csx);
#endif
            status = ERROR;
        }
    }

    return status;
}

//================== Power Management ====================================

static void epdSetPwrToPmic(unsigned char *pmicData)
{
    unsigned int buf;

    buf = voltageToRegisterData(pmicData[0], SET_VPOS_VENG2);
    initPmicData[4] = buf / 256;
    initPmicData[5] = buf % 256;

    buf = voltageToRegisterData(pmicData[1], SET_VPOS_VENG3);
    initPmicData[8] = buf / 256;
    initPmicData[9] = buf % 256;

    buf = voltageToRegisterData(pmicData[2], SET_VPOS_VENG2);
    initPmicData[6] = buf / 256;
    initPmicData[7] = buf % 256;

    buf = voltageToRegisterData(pmicData[3], SET_VPOS_VENG3);
    initPmicData[10] = buf / 256;
    initPmicData[11] = buf % 256;

    buf = voltageToRegisterData(pmicData[4], SET_VCOMDC);
    initPmicData[12] = buf / 256;
    initPmicData[13] = buf % 256;
}

//================== Display Functions ====================================

void epdDisplay(void)
{
#if SHOW_LOG
    printf("Turn on PMIC.\n");
#endif
    enablePmic();

#if SHOW_LOG
    printf("Write PON\n");
#endif
    epdSetCsAll(GPIO_LOW);
    epdWriteCommand(EPD_CMD_PON);
    epdCheckBusyHigh();
    epdSetCsAll(GPIO_HIGH);

#if SHOW_LOG
    printf("Write DRF\n");
#endif
    epdSetCsAll(GPIO_LOW);
    delayms(30);
    epdWrite(EPD_CMD_DRF, DRF_V, sizeof(DRF_V));
    epdCheckBusyHigh();
    epdSetCsAll(GPIO_HIGH);

#if SHOW_LOG
    printf("Write POF\n");
#endif
    epdSetCsAll(GPIO_LOW);
    epdWrite(EPD_CMD_POF, POF_V, sizeof(POF_V));
    epdSetCsAll(GPIO_HIGH);

#if SHOW_LOG
    printf("Turn off PMIC.\n");
#endif
    disablePmic();

#if SHOW_LOG
    printf("Display Done!!\n");
#endif
}

void epdDisplayColor(unsigned char colorSelect)
{
    unsigned long i;

    printf("Displaying solid color: 0x%02X\n", colorSelect);

    memset(epdImageDataBuffer, colorSelect, EPD_IMAGE_DATA_BUFFER);

    if(epdSetPower() == DONE)
    {
        epdSetCsAll(GPIO_LOW);
        epdWriteCommand(EPD_CMD_DTM);

        // Send image data in chunks
        for(i = 0; i < 288000 / EPD_IMAGE_DATA_BUFFER; i++)
        {
            epdWriteData(epdImageDataBuffer, EPD_IMAGE_DATA_BUFFER);
        }
        epdWriteData(epdImageDataBuffer, 288000 % EPD_IMAGE_DATA_BUFFER);
        epdSetCsAll(GPIO_HIGH);

        epdDisplay();

#if SHOW_LOG
        printf("Display color complete.\n");
#endif
    }
    else
    {
#if SHOW_LOG
        printf("Display color does not work due to epdSetPower() NG.\n");
#endif
    }
}

void epdDisplayColorBar(void)
{
    unsigned long i;

    printf("Displaying color bar pattern...\n");

    if(epdSetPower() == DONE)
    {
        epdSetCsAll(GPIO_LOW);
        epdWriteCommand(EPD_CMD_DTM);

        // BLACK (48000 bytes = 1/6 of screen)
        memset(epdImageDataBuffer, EPD_BLACK, EPD_IMAGE_DATA_BUFFER);
        for(i = 0; i < 48000 / EPD_IMAGE_DATA_BUFFER; i++)
        {
            epdWriteData(epdImageDataBuffer, EPD_IMAGE_DATA_BUFFER);
        }
        epdWriteData(epdImageDataBuffer, 48000 % EPD_IMAGE_DATA_BUFFER);

        // WHITE
        memset(epdImageDataBuffer, EPD_WHITE, EPD_IMAGE_DATA_BUFFER);
        for(i = 0; i < 48000 / EPD_IMAGE_DATA_BUFFER; i++)
        {
            epdWriteData(epdImageDataBuffer, EPD_IMAGE_DATA_BUFFER);
        }
        epdWriteData(epdImageDataBuffer, 48000 % EPD_IMAGE_DATA_BUFFER);

        // YELLOW
        memset(epdImageDataBuffer, EPD_YELLOW, EPD_IMAGE_DATA_BUFFER);
        for(i = 0; i < 48000 / EPD_IMAGE_DATA_BUFFER; i++)
        {
            epdWriteData(epdImageDataBuffer, EPD_IMAGE_DATA_BUFFER);
        }
        epdWriteData(epdImageDataBuffer, 48000 % EPD_IMAGE_DATA_BUFFER);

        // RED
        memset(epdImageDataBuffer, EPD_RED, EPD_IMAGE_DATA_BUFFER);
        for(i = 0; i < 48000 / EPD_IMAGE_DATA_BUFFER; i++)
        {
            epdWriteData(epdImageDataBuffer, EPD_IMAGE_DATA_BUFFER);
        }
        epdWriteData(epdImageDataBuffer, 48000 % EPD_IMAGE_DATA_BUFFER);

        // BLUE
        memset(epdImageDataBuffer, EPD_BLUE, EPD_IMAGE_DATA_BUFFER);
        for(i = 0; i < 48000 / EPD_IMAGE_DATA_BUFFER; i++)
        {
            epdWriteData(epdImageDataBuffer, EPD_IMAGE_DATA_BUFFER);
        }
        epdWriteData(epdImageDataBuffer, 48000 % EPD_IMAGE_DATA_BUFFER);

        // GREEN
        memset(epdImageDataBuffer, EPD_GREEN, EPD_IMAGE_DATA_BUFFER);
        for(i = 0; i < 48000 / EPD_IMAGE_DATA_BUFFER; i++)
        {
            epdWriteData(epdImageDataBuffer, EPD_IMAGE_DATA_BUFFER);
        }
        epdWriteData(epdImageDataBuffer, 48000 % EPD_IMAGE_DATA_BUFFER);

        epdSetCsAll(GPIO_HIGH);

        epdDisplay();

#if SHOW_LOG
        printf("Display color bar complete.\n");
#endif
    }
    else
    {
#if SHOW_LOG
        printf("Display color bar does not work due to epdSetPower() NG.\n");
#endif
    }
}

void epdWriteImage(unsigned char csx, const unsigned char *imageData, unsigned long imageDataLength)
{
    printf("Writing image data to driver IC %d...\n", csx);

    epdSetCs(csx, GPIO_LOW);
    spiTransmitLargeData(EPD_CMD_DTM, (unsigned char *)imageData, imageDataLength);
    epdSetCs(csx, GPIO_HIGH);

#if SHOW_LOG
    printf("Writing data is completed.\n");
#endif
}


unsigned char epdSetPower(void)
{
  unsigned char i, vcomStatus=DONE;
  unsigned char readTscBuf[2],readPwrBuf[5];

  //Read TSC
  epdSetCs(0,GPIO_LOW);
  epdRead(EPD_CMD_TSC, &readTscBuf[0], sizeof(readTscBuf));
  epdCheckBusyHigh();
  epdSetCs(0,GPIO_HIGH);
#if SHOW_LOG
  printf("TSC Data = 0x%02X, 0x%02X \r\n", readTscBuf[0], readTscBuf[1]);
#endif

  //PON without the external power
  epdSetCs(0,GPIO_LOW);
  epdWriteCommand(EPD_CMD_PON);
  epdCheckBusyHigh();
  epdSetCs(0,GPIO_HIGH);

  //Read PWR
  epdSetCs(0,GPIO_LOW);
  epdRead(0x9B, &readPwrBuf[0], 4);
  epdSetCs(0,GPIO_HIGH);
#if SHOW_LOG
  printf("PWR Data = 0x%02X, 0x%02X 0x%02X, 0x%02X \r\n", readPwrBuf[0], readPwrBuf[1], readPwrBuf[2], readPwrBuf[3]);
#endif

  //POF
  epdSetCs(0,GPIO_LOW);
  epdWriteCommand(EPD_CMD_POF);
  epdCheckBusyHigh();
  epdSetCs(0,GPIO_HIGH);
  
  //Read VCOM
  epdSetCs(0,GPIO_LOW);
  epdRead(0x8A, &readPwrBuf[4], 1);
  epdSetCs(0,GPIO_HIGH);


  if(readPwrBuf[4]== 0x00)
  {
#if SHOW_LOG
  printf("VCOM Data = 0x%02X \r\n", readPwrBuf[4]);  
  printf("Data NG! \r\n");
#endif
    vcomStatus = ERROR;
  }
  else
  {
    for(i=0;i<4;i++)
    {
      if(readPwrBuf[i] > 120)
      {
        vcomStatus = ERROR;
#if SHOW_LOG
        printf("PWM Data [%d] = 0x%02X \r\n",i , readPwrBuf[4]);  
        printf("Data NG! \r\n");
#endif
      }
    }

    if(vcomStatus == DONE)
    {
      readPwrBuf[4] = readPwrBuf[4] - 128;
      epdSetPwrToPmic(readPwrBuf);
#if SHOW_LOG
      printf("PWM data and VCOM data are both in range! \r\n");
#endif
    }

  }
  
  return vcomStatus;

}
