#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include "TCA9555.h"
#include "pindefine.h"
#include "globals.h"
#include "pmic.h"
#include "status.h"
#include "BfkLx0.h"
#include "esp_sleep.h"

extern "C"
{
#include "epd.h"
#include "comm.h"
}

// Define pins for ESP32-C5
// IMPORTANT: ESP32-C5 has specific GPIO restrictions
// Common valid I2C pins for ESP32-C5: GPIO 6/7, GPIO 8/9, GPIO 18/19
// GPIO 2/3 may cause ESP_ERR_INVALID_STATE on some ESP32-C5 boards
#define I2C_SDA 2 // Try GPIO 6 for SDA (change if needed)
#define I2C_SCL 3 // Try GPIO 7 for SCL (change if needed)

// Interrupt pins from TCA9555 expanders
// #define GPIO_INT1 28 // Interrupt from first expander
// #define GPIO_INT2 27 // Interrupt from second expander

// Create two TCA9555 objects with different I2C addresses
// Default address is 0x20 (when A0, A1, A2 are all LOW)
// Second device should have different address (set via hardware pins A0, A1, A2)
TCA9555 expander1(0x20); // First expander at default address
TCA9555 expander2(0x21); // Second expander (adjust address based on hardware)

// Interrupt flags
volatile bool expander1_interrupt = false;
volatile bool expander2_interrupt = false;

// void expanders_voltage_gpio_init()
// {
//   expander1.pinMode1(EXP1_EN_V5A, OUTPUT);
//   expander2.pinMode1(EXP2_EN_NEGATIVE_3_5V, OUTPUT);
//   expander2.pinMode1(EXP2_EN_1_35V, OUTPUT);
//   expander2.pinMode1(EXP2_EN_V5D, OUTPUT);
//   expander2.pinMode1(EXP2_EN_DC12, OUTPUT);
//   expander2.pinMode1(EXP2_EN_DC19, OUTPUT);
// }

void gpio_cs_pins_init() // set all pins HIGH
{
  // Set all CS pins HIGH (inactive)
  expander1.write1(EXP1_SPI1_CS0, HIGH);
  expander1.write1(EXP1_SPI1_CS1, HIGH);
  expander1.write1(EXP1_SPI1_CS2, HIGH);
  expander1.write1(EXP1_SPI1_CS3, HIGH);
  expander1.write1(EXP1_SPI2_CS0, HIGH);
  expander1.write1(EXP1_SPI2_CS1, HIGH);
  expander1.write1(EXP1_SPI2_CS2, HIGH);
  expander1.write1(EXP1_SPI2_CS3, HIGH);
  // Initialize all SPI chip select pins as outputs (active low, so start HIGH)
  expander1.pinMode1(EXP1_SPI1_CS3, OUTPUT);
  expander1.pinMode1(EXP1_SPI1_CS2, OUTPUT);
  expander1.pinMode1(EXP1_SPI1_CS1, OUTPUT);
  expander1.pinMode1(EXP1_SPI1_CS0, OUTPUT);
  expander1.pinMode1(EXP1_SPI2_CS3, OUTPUT);
  expander1.pinMode1(EXP1_SPI2_CS2, OUTPUT);
  expander1.pinMode1(EXP1_SPI2_CS1, OUTPUT);
  expander1.pinMode1(EXP1_SPI2_CS0, OUTPUT);
}

void epd_gpio_init()
{
  // Initialize EPD control pins on ESP32-C5
  // pinMode(EPD_RST, OUTPUT);

  pinMode(EPD_BUSY, INPUT);

  // Set reset pin HIGH (not in reset)
  // digitalWrite(EPD_RST, HIGH);
  expander2.write1(EXP2_EPD_RST, HIGH);
  delay(50);
  expander2.pinMode1(EXP2_EPD_RST, OUTPUT);
  delay(50);
  Serial.println("EPD GPIO initialized: RST=OUTPUT, BUSY=INPUT");
}

void printWakeupReason()
{
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  switch (cause)
  {
  case ESP_SLEEP_WAKEUP_GPIO:
    Serial.println("[SLEEP] Wakeup cause: GPIO (IO27 / INT2)");
    break;
  case ESP_SLEEP_WAKEUP_UNDEFINED:
    Serial.println("[SLEEP] Wakeup cause: Power-on / reset (not deep sleep)");
    break;
  default:
    Serial.printf("[SLEEP] Wakeup cause: %d\n", (int)cause);
    break;
  }
}

void enterDeepSleep()
{
  Serial.println("[SLEEP] Configuring IO as deep sleep wakeup source (active LOW)...");
  expander2.pinMode1(EXP2_EN_DC12, INPUT);
  delay(50);
  expander1.pinMode1(EXP1_VDDP_EN, INPUT);
  expander1.pinMode1(EXP1_VDDN_EN, INPUT);
  delay(50);
  setGpioLevel_EXPANDER_2(EXP2_EN_VIN_DC12, LOW);
  delay(50);
  expander1.pinMode1(EXP1_VNCP_EN, INPUT);
  delay(50);
  expander1.write1(EXP1_EN_V5A, LOW);
  delay(50);
  expander2.write1(EXP2_EN_V5D, LOW);
  delay(50);
  expander1.pinMode1(EXP1_EN_V5A, OUTPUT);
  delay(50);
  expander2.pinMode1(EXP2_EN_V5D, OUTPUT);
  delay(50);
  expander2.pinMode1(EXP2_EPD_RST, INPUT);
  delay(50);
  expander1.pinMode1(EXP1_PMIC_PS, INPUT);
  expander2.pinMode1(EXP2_EPD_RST, INPUT);
  expander1.pinMode1(EXP1_EN_V5A, INPUT);
  expander2.pinMode1(EXP2_EN_V5D, INPUT);
  expander2.pinMode1(EXP2_EN_NEGATIVE_3_5V, INPUT);
  expander2.pinMode1(EXP2_EN_DC12, INPUT);
  expander2.pinMode1(EXP2_EN_DC19, INPUT);
  expander1.pinMode1(EXP1_VNCP_EN, INPUT);
  expander1.pinMode1(EXP1_VDDP_EN, INPUT);
  expander1.pinMode1(EXP1_VDDN_EN, INPUT);
  delay(50);
  expander1.pinMode1(EXP1_SPI1_CS3, INPUT);
  expander1.pinMode1(EXP1_SPI1_CS2, INPUT);
  expander1.pinMode1(EXP1_SPI1_CS1, INPUT);
  expander1.pinMode1(EXP1_SPI1_CS0, INPUT);
  expander1.pinMode1(EXP1_SPI2_CS3, INPUT);
  expander1.pinMode1(EXP1_SPI2_CS2, INPUT);
  expander1.pinMode1(EXP1_SPI2_CS1, INPUT);
  expander1.pinMode1(EXP1_SPI2_CS0, INPUT);
  delay(50);
  expander2.pinMode1(EXP2_EN_VIN_SCREEN_3V3, INPUT);
  delay(50);
  pinMode(0, INPUT);
  pinMode(1, INPUT);
  pinMode(2, INPUT);
  pinMode(3, INPUT);
  pinMode(4, INPUT);
  pinMode(5, INPUT);
  pinMode(6, INPUT);
  pinMode(7, INPUT);
  pinMode(8, INPUT);
  pinMode(9, INPUT);
  pinMode(10, INPUT);
  pinMode(13, INPUT);
  pinMode(14, INPUT);
  pinMode(23, INPUT);
  pinMode(24, INPUT);
  pinMode(25, INPUT);
  pinMode(26, INPUT);
  pinMode(27, INPUT);
  pinMode(28, INPUT);
  delay(100);
  esp_err_t err = esp_deep_sleep_enable_gpio_wakeup(1ULL << 0, ESP_GPIO_WAKEUP_GPIO_LOW);
  esp_err_t err1 = esp_deep_sleep_enable_gpio_wakeup(1ULL << 1, ESP_GPIO_WAKEUP_GPIO_LOW);
  esp_err_t err2 = esp_deep_sleep_enable_gpio_wakeup(1ULL << 4, ESP_GPIO_WAKEUP_GPIO_LOW);
  // esp_err_t err3 = esp_deep_sleep_enable_gpio_wakeup(1ULL << 5, ESP_GPIO_WAKEUP_GPIO_LOW);
  if (err != ESP_OK)
  {
    Serial.printf("[SLEEP] ERROR: Failed to configure GPIO wakeup: %d\n", err);
    return;
  }

  Serial.println("[SLEEP] Entering deep sleep. Will wake on IO27 LOW.");
  Serial.flush();
  esp_deep_sleep_start();
}

// Interrupt service routines
void IRAM_ATTR expander1_ISR()
{
  expander1_interrupt = true;
}

void IRAM_ATTR expander2_ISR()
{
  expander2_interrupt = true;
}

// I2C Scanner function to find all devices on the bus
void scanI2C()
{
  byte error, address;
  int deviceCount = 0;

  Serial.println("\n========== I2C Bus Scanner ==========");
  Serial.println("Scanning I2C bus (addresses 0x01 to 0x7F)...\n");

  for (address = 1; address < 127; address++)
  {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0)
    {
      Serial.print("Device found at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.print(address, HEX);
      Serial.print(" (");
      Serial.print(address);
      Serial.print(")");

      // Identify known devices
      if (address >= 0x20 && address <= 0x27)
      {
        Serial.print(" - Likely TCA9555/TCA9535");
        if (address == 0x20)
          Serial.print(" (Expander 1)");
        if (address == 0x21)
          Serial.print(" (Expander 2)");
      }
      else
      {
        Serial.print(" - Unknown device (possibly IST9201?)");
      }
      Serial.println();
      deviceCount++;
    }
    else if (error == 4)
    {
      Serial.print("Unknown error at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.println(address, HEX);
    }
  }

  Serial.println("\n====================================");
  if (deviceCount == 0)
  {
    Serial.println("No I2C devices found!");
    Serial.println("Check connections and pull-up resistors.");
  }
  else
  {
    Serial.print("Total devices found: ");
    Serial.println(deviceCount);
  }
  Serial.println("====================================\n");
}

void setup()
{
  Serial.begin(115200);
  while (!Serial && millis() < 5000)
  {
    delay(10);
  }

  if (!Wire.begin(I2C_SDA, I2C_SCL, 100000))
  {
    Serial.println("ERROR: Wire.begin() with frequency FAILED!");
    Serial.println("Trying without frequency parameter...");

    if (!Wire.begin(I2C_SDA, I2C_SCL))
    {
      Serial.println("ERROR: Wire.begin() FAILED!");
      Serial.println("This is ESP_ERR_INVALID_STATE - pins may not support I2C");
      while (1)
      {
        delay(1000);
      }
    }
  }

  pinMode(EXP_EN, OUTPUT);
  delay(10);
  digitalWrite(EXP_EN, HIGH);
  delay(100);
  expander2.write1(12, LOW);//test
  expander2.pinMode1(12, OUTPUT);//test
  pinMode(PMIC_PG, INPUT); // PG io 26
  delay(50);
  pinMode(EPD_BUSY, INPUT); // busy io 27
  delay(50);
  pinMode(SPI_1DSX, INPUT); // 1DSX io 23
  delay(50);
  // pinMode(SPI_2DSX, INPUT); // 2DSX io 24
  // delay(50);
  pinMode(6, INPUT);
  delay(50);
  pinMode(7, INPUT);
  delay(50);
  pinMode(8, INPUT);
  delay(50);
  pinMode(9, INPUT);
  delay(50);
  pinMode(10, INPUT);
  delay(50);
  expander2.pinMode1(EXP2_EPD_RST, INPUT);
  expander1.pinMode1(EXP1_EN_V5A, INPUT);
  expander2.pinMode1(EXP2_EN_V5D, INPUT);
  expander2.pinMode1(EXP2_EN_NEGATIVE_3_5V, INPUT);
  expander2.pinMode1(EXP2_EN_DC12, INPUT);
  expander2.pinMode1(EXP2_EN_DC19, INPUT);
  expander1.pinMode1(EXP1_VNCP_EN, INPUT);
  expander1.pinMode1(EXP1_VDDP_EN, INPUT);
  expander1.pinMode1(EXP1_VDDN_EN, INPUT);
  expander1.pinMode1(EXP1_SPI1_CS3, INPUT);
  // delay(50);
  expander1.pinMode1(EXP1_SPI1_CS2, INPUT);
  // delay(50);
  expander1.pinMode1(EXP1_SPI1_CS1, INPUT);
  // delay(50);
  expander1.pinMode1(EXP1_SPI1_CS0, INPUT);
  // delay(50);
  expander1.pinMode1(EXP1_SPI2_CS3, INPUT);
  // delay(50);
  expander1.pinMode1(EXP1_SPI2_CS2, INPUT);
  // delay(50);
  expander1.pinMode1(EXP1_SPI2_CS1, INPUT);
  // delay(50);
  expander1.pinMode1(EXP1_SPI2_CS0, INPUT);
  // pinMode(PMIC_PS, OUTPUT);   // PS expander1 p15
  // digitalWrite(PMIC_PS, LOW); // PS expander1 p15
  expander1.write1(EXP1_PMIC_PS, LOW);
  expander1.pinMode1(EXP1_PMIC_PS, OUTPUT);
  // pinMode(TOOGLE_SEL1, OUTPUT); // not used
  // pinMode(TOOGLE_SEL2, OUTPUT); // not used

  // digitalWrite(TOOGLE_SEL1, LOW);// not used
  // digitalWrite(TOOGLE_SEL2, LOW);// not used
  pinMode(PMIC_EN, OUTPUT); // PS expander1 p15
  digitalWrite(PMIC_EN, LOW);
  // expanders_voltage_gpio_init();
  // pinMode(25, OUTPUT); // ist EN
  // digitalWrite(25, LOW);
  // expander1.write1(EXP1_PMIC_EN, LOW);// changed
  // expander1.pinMode1(EXP1_PMIC_EN, OUTPUT);// changed

  Serial.println("Setting voltage enable pins HIGH...");
  // expander1.write1(EXP1_EN_V5A, HIGH);
  // expander2.write1(EXP2_EN_V5D, HIGH);
  // expander1.pinMode1(EXP1_EN_V5A, OUTPUT);
  // expander2.pinMode1(EXP2_EN_V5D, OUTPUT);
  // expander2.write1(EXP2_EN_1_35V, HIGH);
  // delay(50);
  // expander2.pinMode1(EXP2_EN_1_35V, OUTPUT);
  setGpioLevel_EXPANDER_2(EXP2_EN_VIN_SCREEN_3V3, GPIO_HIGH);
  delay(50);
  setGpioLevel(EXP1_VNCP_EN, GPIO_HIGH);
  // Serial.println("-3.5v ON MOSFET");
  // Serial.println("5v");
  delay(1000);
  Serial.println("EN-SWITCH");
  setGpioLevel_EXPANDER_2(EXP2_EN_VIN_DC12, GPIO_HIGH);
  delay(500);
  // setGpioLevel_EXPANDER_2(EXP2_EN_VIN_SCREEN_3V3, GPIO_HIGH);
  delay(500);
  expander2.write1(EXP2_EN_DC12, HIGH);
  expander2.pinMode1(EXP2_EN_DC12, OUTPUT);
  Serial.println("12v");
  delay(1000);
  expander1.write1(EXP1_EN_V5A, HIGH);
  expander2.write1(EXP2_EN_V5D, HIGH);
  expander1.pinMode1(EXP1_EN_V5A, OUTPUT);
  expander2.pinMode1(EXP2_EN_V5D, OUTPUT);
  delay(500);

  scanI2C();
  // Read PMIC registers 0x1D and 0x1E
  printf("\n========== Reading PMIC Registers ==========\n");

  unsigned char reg_data_0x1D = 0;
  unsigned char reg_data_0x1E = 0;

  // Read 1 byte from register 0x1D
  printf("Reading from PMIC register 0x1D...\n");
  if (readPmicRegister(0x1D, &reg_data_0x1D, 1) == DONE)
  {
    printf("Register 0x1D: 0x%02X (%d)\n", reg_data_0x1D, reg_data_0x1D);
  }

  printf("\n");

  // Read 1 byte from register 0x1E
  printf("Reading from PMIC register 0x1E...\n");
  if (readPmicRegister(0x1E, &reg_data_0x1E, 1) == DONE)
  {
    printf("Register 0x1E: 0x%02X (%d)\n", reg_data_0x1E, reg_data_0x1E);
  }

  printf("===========================================\n\n");
  // delay(500);
  // powerSwitchEnable(); // tetst
  //  // Initialize EPD GPIO pins
  epd_gpio_init();
  // // Initialize SPI CS pins on expander
  gpio_cs_pins_init();
  // // Initialize SPI
  spiInit();
  epdHardwareReset();
  // Initialize EPD driver ICs
  printf("\n========== Initializing EPD ==========\n");

  // Allocate one PSRAM buffer per color and pre-fill
  const uint8_t colorValues[] = {EPD_BLACK, EPD_WHITE, EPD_YELLOW, EPD_RED, EPD_BLUE, EPD_GREEN};
  const char *colorNames[] = {"BLACK", "WHITE", "YELLOW", "RED", "BLUE", "GREEN"};
  const int numColors = 6;
  unsigned char *colorBuffers[numColors];

  for (int i = 0; i < numColors; i++)
  {
    colorBuffers[i] = (unsigned char *)ps_malloc(EPD_IMAGE_DATA_BUFFER);
    if (colorBuffers[i] == nullptr)
    {
      printf("[EPD] FATAL: ps_malloc failed for %s buffer!\n", colorNames[i]);
      while (1)
        ;
    }
    memset(colorBuffers[i], colorValues[i], EPD_IMAGE_DATA_BUFFER);
    printf("[EPD] %s buffer ready in PSRAM (%u bytes)\n", colorNames[i], EPD_IMAGE_DATA_BUFFER);
  }
  delayms(10);
  // Check driver IC status
  if (epdCheckDriverICStatus() == DONE)
  {
    epdInit();

    if (epdSetPower() == DONE)
    {
      epdWriteImage(0, BfkLx0_sections[0], EPD_IMAGE_DATA_BUFFER);
      epdWriteImage(1, BfkLx0_sections[1], EPD_IMAGE_DATA_BUFFER);
      epdWriteImage(2, BfkLx0_sections[2], EPD_IMAGE_DATA_BUFFER);
      epdWriteImage(3, BfkLx0_sections[3], EPD_IMAGE_DATA_BUFFER);
      epdWriteImage(4, BfkLx0_sections[4], EPD_IMAGE_DATA_BUFFER);
      epdWriteImage(5, BfkLx0_sections[5], EPD_IMAGE_DATA_BUFFER);
      epdWriteImage(6, BfkLx0_sections[6], EPD_IMAGE_DATA_BUFFER);
      epdWriteImage(7, BfkLx0_sections[7], EPD_IMAGE_DATA_BUFFER);
      epdDisplay();
      printf("Display complete.\n");
      delay(2000);
    }
  }
  else
  {
    printf("WARNING: Some driver ICs did not respond\n");
  }
  powerSwitchDisable();
  printf("======================================\n\n");
  enterDeepSleep();
}
void loop()
{
}