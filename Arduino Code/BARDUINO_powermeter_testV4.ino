#include <SPI.h>
#include "lowpass.h"
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_NeoPixel.h>

//setup neopixel
#define PIN 38  // On Trinket or Gemma, suggest changing this to 1
#define NUMPIXELS 1  // Popular NeoPixel ring size

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

//setup for wifi passwords
const char* ssid = "wifi 1234";      // Replace with your WiFi SSID
const char* password = "12345";  // Replace with your WiFi password
const char* nodeRedEndpoint = "https://noderedendpointhere";  // Replace with your Node-RED endpoint or any server endpoint for recieving consumption data


/*
  Irms conversion ratio derived from
  CT number of turns / Burden resistor
  Adjust this value to calibrate - make sure to
  use decimal places to avoid integer division!
*/
float turns_ratio = 1860.00;
float burdenresistor = 47.00;
float input = 100;
float output = 1;

const float current_ratio = input/output;

int16_t adc_bias = 320;


// Actual Arduino voltage measured across 5V pin & GND
#define ARDUINO_V 3.3 // is 4.8with uno

// Electricity cost per kWHr, not including standing charge
#define COST_PER_KWHR 25.00  // Cost per kWHr (cents)

// Mains RMS voltage - can be adjusted
#define MAINS_V_RMS  230.0

#define NUM_OF_SAMPLES 1600
#define ADC_PIN 1


// define pins for screen
#if defined(ARDUINO_FEATHER_ESP32) // Feather Huzzah32
  #define TFT_CS         14
  #define TFT_RST        15
  #define TFT_DC         32

#elif defined(ESP8266)
  #define TFT_CS         4
  #define TFT_RST        16                                            
  #define TFT_DC         5

#else
  // For the breakout board, you can use any 2 or 3 pins.
  // These pins will also work for the 1.8" TFT shield.
  #define TFT_CS        10
  #define TFT_RST        9 // Or set to -1 and connect to Arduino RESET pin
  #define TFT_DC         8
#endif

// setup screen for use
// For 1.14", 1.3", 1.54", 1.69", and 2.0" TFT with ST7789:
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
float p = 3.1415926;

// Variable to store total energy used + start at 0
int total_energy = 0.0;
double active_power = 0.0;
double rnd_cost= 0.0;
int interval_energy = 0.0;


void setup() {
  Serial.begin(115200);

  // seting up screen
  Serial.print(F("Hello! Screen Initializing"));
  tft.init(135, 240);           // Init ST7789 240x135
  Serial.println("done");

  //setting up wifi
  pixels.begin();  // INITIALIZE NeoPixel strip object (REQUIRED)
  WiFi.begin(ssid, password);
  
    Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    pixels.setPixelColor(0, pixels.Color(255, 0, 0));
    pixels.show();
    delay(500);
    pixels.setPixelColor(0, pixels.Color(0, 0, 0));
    pixels.show();
    delay(500);
    tftInitial();
  }
  Serial.println("WiFi connected");
  pixels.setPixelColor(0, pixels.Color(0, 255, 0));
  pixels.show();
}


void loop() {

  unsigned long start = millis();

  int32_t adc_sum_sqr = 0;
  for (int n = 0; n < NUM_OF_SAMPLES; n++)
  {
    int16_t adc_raw = analogRead(ADC_PIN);
    //Serial.print("adc raw is");
    //Serial.println(adc_raw);

    // Remove ADC bias - low pass filter
    int16_t adc_filtered = intLowPass(&adc_bias, adc_raw);

    // Accumulate square of filtered ADC readings
    adc_sum_sqr += adc_filtered * adc_filtered;
  }
  unsigned long finish = millis();

  // Calculate measured RMS (Root Mean Square) voltage across burden resistor
  double vrms = sqrt(adc_sum_sqr / NUM_OF_SAMPLES)* ARDUINO_V / 1024.0; //with barduino check bit rate

  // Scale to mains current RMS (measured vrms * CT turns / Burden R)
  double Irms = vrms * current_ratio;


  // Calculate active power (Irms * Vrms)
  double active_power = Irms * MAINS_V_RMS;

  // Calculate cost per kWHr
  rnd_cost = round(active_power * COST_PER_KWHR / 1000.0);

  // Calculate the energy for this interval in watt-hours
  int interval_duration_hours = (finish - start) / 3600000.0; // Convert milliseconds to hours
  int interval_energy = active_power * interval_duration_hours;
  total_energy += interval_energy;

    
  // tft print function!
  tftPrintTest();


  Serial.print("Arduino V: ");
  Serial.println(ARDUINO_V);
  Serial.print(" Accumulated ADC: ");
  Serial.println(adc_sum_sqr);
  Serial.print(" Samples: ");
  Serial.println(NUM_OF_SAMPLES);
  Serial.print(" Measured V: ");
  Serial.println(vrms, 3);
  Serial.print(" Irms: ");
  Serial.println(Irms);
  Serial.print(" Active Power: ");
  Serial.println(active_power);
  Serial.print(" Rnd Cost: ");
  Serial.println(rnd_cost);
  Serial.print(" Time: ");
  Serial.println(finish - start);

  int runtime = finish - start;
  
  // Displaying the total energy used
  Serial.print("Total Energy Used (Wh): ");
  Serial.println(total_energy);

  //send energy used today to nodered
  sendDataToNodeRed(active_power); //change active_power to total_energy when up and running!!(int today_energy, int IRMS, int RND cost, int runtime) {
  delay(2000);
}

void tftInitial(){
  tft.setTextWrap(false);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setRotation(-3);
  tft.setCursor(0, 10);
  tft.println("Setting Up...");
}

void tftPrintTest() {
  tft.setTextWrap(false);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);

  // Rotate the display to landscape mode
  tft.setRotation(-3); // Adjust the rotation value as needed
  
  // Print Active Power
  tft.setCursor(0, 10);
  tft.println("Active Power: ");
  tft.print(active_power);
  tft.println(" Watts");
  
  // Print Todays Energy
  tft.setCursor(0, 50);
  tft.println("Todays Energy: ");
  tft.print(total_energy);
  tft.println(" Watts");
  
  // Print Electrical Cost
  tft.setCursor(0, 90);
  tft.println("Electrical Cost: ");
  tft.print(rnd_cost);
  tft.println(" Cents");
}

// function to send to nodered

void sendDataToNodeRed(int today_energy) {
  HTTPClient http;
  http.begin(nodeRedEndpoint);
  http.addHeader("Content-Type", "application/json");

  String payload = "{\"today_energy\":" + String(today_energy, 2) + "}";

  int httpResponseCode = http.POST(payload);

  if (httpResponseCode == 200) {
    Serial.println("Data sent to Node-RED dashboard successfully.");
  } else {
    Serial.print("Failed to send data to Node-RED dashboard. Status code: ");
    Serial.println(httpResponseCode);
  }
  
  http.end();
}
