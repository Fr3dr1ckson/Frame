#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include "TCA9555.h"
#include "pindefine.h"
#include "globals.h"
#include "pmic.h"
#include "status.h"

extern "C" {
#include "epd.h"
#include "comm.h"
}


// Define pins for ESP32-C5
// IMPORTANT: ESP32-C5 has specific GPIO restrictions
// Common valid I2C pins for ESP32-C5: GPIO 6/7, GPIO 8/9, GPIO 18/19
// GPIO 2/3 may cause ESP_ERR_INVALID_STATE on some ESP32-C5 boards
#define I2C_SDA 2  // Try GPIO 6 for SDA (change if needed)
#define I2C_SCL 3  // Try GPIO 7 for SCL (change if needed)

// Interrupt pins from TCA9555 expanders
#define GPIO_INT1 28  // Interrupt from first expander
#define GPIO_INT2 27  // Interrupt from second expander


// Create two TCA9555 objects with different I2C addresses
// Default address is 0x20 (when A0, A1, A2 are all LOW)
// Second device should have different address (set via hardware pins A0, A1, A2)
TCA9555 expander1(0x20);  // First expander at default address
TCA9555 expander2(0x21);  // Second expander (adjust address based on hardware)



// Interrupt flags
volatile bool expander1_interrupt = false;
volatile bool expander2_interrupt = false;


void expanders_voltage_gpio_init(){
  expander1.pinMode1(EXP1_EN_V5A,OUTPUT);
  expander2.pinMode1(EXP2_EN_NEGATIVE_3_5V,OUTPUT);
  expander2.pinMode1(EXP2_EN_1_35V,OUTPUT);
  expander2.pinMode1(EXP2_EN_V5D,OUTPUT);
  expander2.pinMode1(EXP2_EN_DC12, OUTPUT);
  expander2.pinMode1(EXP2_EN_DC19,OUTPUT);
}

void gpio_cs_pins_init(){
  // Initialize all SPI chip select pins as outputs (active low, so start HIGH)
  expander1.pinMode1(EXP1_SPI1_CS3, OUTPUT);
  expander1.pinMode1(EXP1_SPI1_CS2, OUTPUT);
  expander1.pinMode1(EXP1_SPI1_CS1, OUTPUT);
  expander1.pinMode1(EXP1_SPI1_CS0, OUTPUT);
  expander1.pinMode1(EXP1_SPI2_CS3, OUTPUT);
  expander1.pinMode1(EXP1_SPI2_CS2, OUTPUT);
  expander1.pinMode1(EXP1_SPI2_CS1, OUTPUT);
  expander1.pinMode1(EXP1_SPI2_CS0, OUTPUT);

  // Set all CS pins HIGH (inactive)
  expander1.write1(EXP1_SPI1_CS0, HIGH);
  expander1.write1(EXP1_SPI1_CS1, HIGH);
  expander1.write1(EXP1_SPI1_CS2, HIGH);
  expander1.write1(EXP1_SPI1_CS3, HIGH);
  expander1.write1(EXP1_SPI2_CS0, HIGH);
  expander1.write1(EXP1_SPI2_CS1, HIGH);
  expander1.write1(EXP1_SPI2_CS2, HIGH);
  expander1.write1(EXP1_SPI2_CS3, HIGH);
}

void epd_gpio_init(){
  // Initialize EPD control pins on ESP32-C5
  pinMode(EPD_RST, OUTPUT);
  pinMode(EPD_BUSY, INPUT);

  // Set reset pin HIGH (not in reset)
  digitalWrite(EPD_RST, HIGH);

  Serial.println("EPD GPIO initialized: RST=OUTPUT, BUSY=INPUT");
}
// Interrupt service routines
void IRAM_ATTR expander1_ISR() {
  expander1_interrupt = true;
}

void IRAM_ATTR expander2_ISR() {
  expander2_interrupt = true;
}

// I2C Scanner function to find all devices on the bus
void scanI2C() {
  byte error, address;
  int deviceCount = 0;

  Serial.println("\n========== I2C Bus Scanner ==========");
  Serial.println("Scanning I2C bus (addresses 0x01 to 0x7F)...\n");

  for(address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("Device found at address 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      Serial.print(" (");
      Serial.print(address);
      Serial.print(")");

      // Identify known devices
      if (address >= 0x20 && address <= 0x27) {
        Serial.print(" - Likely TCA9555/TCA9535");
        if (address == 0x20) Serial.print(" (Expander 1)");
        if (address == 0x21) Serial.print(" (Expander 2)");
      } else {
        Serial.print(" - Unknown device (possibly IST9201?)");
      }
      Serial.println();
      deviceCount++;
    }
    else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
    }
  }

  Serial.println("\n====================================");
  if (deviceCount == 0) {
    Serial.println("No I2C devices found!");
    Serial.println("Check connections and pull-up resistors.");
  } else {
    Serial.print("Total devices found: ");
    Serial.println(deviceCount);
  }
  Serial.println("====================================\n");
}


void setup() {
  Serial.begin(115200);
  byte exp2addr = 0x21;
  byte error;
  delay(2000);  // Longer delay for USB CDC to initialize on ESP32-C5

  // Wait for Serial to be ready (important for USB CDC)
  while (!Serial && millis() < 5000) {
    delay(10);
  }

  Serial.println("\n\n=== ESP32-C5 Starting ===");
  Serial.println("Hello world!");
  Serial.println();

  Serial.println("TCA9555 I/O Expander Test");
  Serial.println("Initializing I2C...");

  // Initialize I2C FIRST before using it
  Serial.print("Calling Wire.begin(");
  Serial.print(I2C_SDA);
  Serial.print(", ");
  Serial.print(I2C_SCL);
  Serial.println(")...");

  // Try with explicit frequency parameter
  if (!Wire.begin(I2C_SDA, I2C_SCL, 100000)) {
    Serial.println("ERROR: Wire.begin() with frequency FAILED!");
    Serial.println("Trying without frequency parameter...");

    if (!Wire.begin(I2C_SDA, I2C_SCL)) {
      Serial.println("ERROR: Wire.begin() FAILED!");
      Serial.println("This is ESP_ERR_INVALID_STATE - pins may not support I2C");
      while(1) { delay(1000); }
    }
  }

  Serial.println("I2C initialized successfully!");
  Serial.print("SDA: GPIO");
  Serial.println(I2C_SDA);
  Serial.print("SCL: GPIO");
  Serial.println(I2C_SCL);

  Wire.beginTransmission(exp2addr);
  error = Wire.endTransmission();
  Serial.print("Error: ");
  Serial.println(error);

  expanders_voltage_gpio_init();

  expander1.pinMode1(EXP1_PMIC_EN, OUTPUT);

  Serial.println("Setting voltage enable pins HIGH...");
  expander2.write1(EXP2_EN_DC12, HIGH);
  expander2.write1(EXP2_EN_DC19,HIGH);
  expander2.write1(EXP2_EN_V5D,HIGH);
  expander2.write1(EXP2_EN_1_35V,HIGH);
  expander2.write1(EXP2_EN_NEGATIVE_3_5V,HIGH);
  expander1.write1(EXP1_EN_V5A,HIGH);

  delay(3000);
  
  expander1.write1(EXP1_PMIC_EN, HIGH);
  enablePmic();

  delay(2000);

  scanI2C();

  // Read PMIC registers 0x1D and 0x1E
  printf("\n========== Reading PMIC Registers ==========\n");

  unsigned char reg_data_0x1D = 0;
  unsigned char reg_data_0x1E = 0;

  // Read 1 byte from register 0x1D
  printf("Reading from PMIC register 0x1D...\n");
  if(readPmicRegister(0x1D, &reg_data_0x1D, 1) == DONE) {
    printf("Register 0x1D: 0x%02X (%d)\n", reg_data_0x1D, reg_data_0x1D);
  }

  printf("\n");

  // Read 1 byte from register 0x1E
  printf("Reading from PMIC register 0x1E...\n");
  if(readPmicRegister(0x1E, &reg_data_0x1E, 1) == DONE) {
    printf("Register 0x1E: 0x%02X (%d)\n", reg_data_0x1E, reg_data_0x1E);
  }

  printf("===========================================\n\n");

  // Initialize EPD GPIO pins
  epd_gpio_init();

  // Initialize SPI CS pins on expander
  gpio_cs_pins_init();

  // Initialize SPI
  spiInit();

  // Initialize EPD driver ICs
  printf("\n========== Initializing EPD ==========\n");
  epdInit();

  // Check driver IC status
  if(epdCheckDriverICStatus() == DONE) {
    printf("All driver ICs are ready!\n");
  } else {
    printf("WARNING: Some driver ICs did not respond\n");
  }
  printf("======================================\n\n");

  // begin() sets all pins as inputs by default
  /*
  if (expander1.begin()) {
    Serial.println("Expander 1 (0x20) initialized successfully");
  } else {
    Serial.println("Failed to initialize Expander 1");
  }

  // Initialize second expander
  if (expander2.begin()) {
    Serial.println("Expander 2 (0x21) initialized successfully");
  } else {
    Serial.println("Failed to initialize Expander 2");
  }

  // Setup interrupt pins
  pinMode(GPIO_INT1, INPUT_PULLUP);
  pinMode(GPIO_INT2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(GPIO_INT1), expander1_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(GPIO_INT2), expander2_ISR, FALLING);
  Serial.println("Interrupts configured on pins 28 and 27");

  // Configure IST9201_EN pin (pin 15 on expander1) as OUTPUT
  expander1.pinMode1(IST9201_EN, OUTPUT);
  Serial.println("IST9201_EN (expander1 pin 15) configured as OUTPUT");

  // Enable IST9201 by setting pin HIGH
  expander1.write1(IST9201_EN, HIGH);
  Serial.println("IST9201_EN set to HIGH (enabled)");

  // Example: Set pins 0-7 of expander1 as outputs, 8-14 as inputs
  // Note: pin 15 is already configured above for IST9201_EN
  expander1.pinMode8(0, 0x00);  // Port 0 (pins 0-7): all outputs (0 = OUTPUT)
  // Port 1 (pins 8-15): pin 15 is output for IST9201_EN, pins 8-14 can be inputs
  // 0x7F = 0b01111111 (bits 0-6 are INPUT, bit 7 is OUTPUT)
  expander1.pinMode8(1, 0x7F);

  // Example: Set pins 0-3 of expander2 as outputs, 4-15 as inputs
  expander2.pinMode8(0, 0xF0);  // Lower 4 bits output, upper 4 input
  expander2.pinMode8(1, 0xFF);  // Port 1: all inputs

  Serial.println("Setup complete");
  */
  
}

void loop() {
  // Check for interrupts from expander1
  /*if (expander1_interrupt) {
    expander1_interrupt = false;
    Serial.println("Expander 1 interrupt detected!");

    // Read the input ports to see what changed
    uint16_t inputs = expander1.read16();
    Serial.print("Expander 1 inputs: 0x");
    Serial.println(inputs, HEX);
  }

  // Check for interrupts from expander2
  if (expander2_interrupt) {
    expander2_interrupt = false;
    Serial.println("Expander 2 interrupt detected!");

    // Read the input ports to see what changed
    uint16_t inputs = expander2.read16();
    Serial.print("Expander 2 inputs: 0x");
    Serial.println(inputs, HEX);
  }

  // Example: Toggle pin 0 of expander1
  static unsigned long lastToggle = 0;
  if (millis() - lastToggle > 1000) {
    lastToggle = millis();
    static bool pinState = false;
    pinState = !pinState;

    expander1.write1(0, pinState ? HIGH : LOW);
    Serial.print("Expander 1, Pin 0: ");
    Serial.println(pinState ? "HIGH" : "LOW");
  }

  // Example: Write pattern to entire port 0 of expander2
  static unsigned long lastPattern = 0;
  if (millis() - lastPattern > 2000) {
    lastPattern = millis();
    static uint8_t pattern = 0;
    expander2.write8(0, pattern++);
    Serial.print("Expander 2, Port 0 pattern: 0x");
    Serial.println(pattern - 1, HEX);
  }
 */   
}