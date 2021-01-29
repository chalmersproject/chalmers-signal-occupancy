//////////////////////////////////////////////////////////
//            CHALMERS SIGNAL OCCUPANCY                 //
//////////////////////////////////////////////////////////
/*
The Chalmers Signal Occuapncy Device is a client tally counter
for shelter reception staff. 

Written an Maintained by the Chalmers project info@chalmersproject.com
F/OSS under M.I.T License
*/

///////////////////////////////////////////////////////////////////////////////////////////
//                     Includes                                                          //
///////////////////////////////////////////////////////////////////////////////////////////
#include <Arduino.h>

//display
#include <SPI.h>
#include <TFT_ILI9163C.h>
#include <Adafruit_GFX.h>
#include <RotaryEncoder.h>

//WiFi and HTTPS requests
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h> // create TLS connection

//LED
#include <FastLED.h>

//
// JSON Support
//
#include <ArduinoJson.h>

//
// API secret and shelter ID
//
#include <shelter_secrets.h>

///////////////////////////////////////////////////////////////////////////////////////////
//                    Toggles                                                            //
///////////////////////////////////////////////////////////////////////////////////////////

// some chalmers signals have red-pcb 1.44" displays from creatron
// others use the cheaper blue-pcb 1.44" displays from aliexpress
static int display_color = 2; //(blue_pcb = 1; red_pcb = 2)

// for debugging it's useful to turn off the chalmer signal's internet-y abilities. That way we can do things like make changes with it's interface without waiting for it to connect to the internet
static bool enable_internet = true;

// earlier versions of chalmers signals don't have their button attached to the ESP. It's useful to be able to quickly turn off all features of the chalmers signal that use this button.
static bool has_button = false;

///////////////////////////////////////////////////////////////////////////////////////////
//                    Globals                                                            //
///////////////////////////////////////////////////////////////////////////////////////////

//
// Measured occupancy value from the chalmers signal
//
int occupancy = 15;
int capacity = 90;
int last_occupancy;
//
// Rotary Encoder Global Variables
//
#define inputCLK 4 // pin
#define inputDT 5
    RotaryEncoder encoder(5, 4);

#include <tft_globals.h>

//
// LED Globals
//
int hue = 0;
uint32 led_last;
#define NUM_LEDS 8
#define DATA_PIN 15
CRGB leds[NUM_LEDS];
int led_brightness = 64;

///////////////////////////////////////////////////////////////////////////////////////////
//                    FUNCTIONS                                                          //
///////////////////////////////////////////////////////////////////////////////////////////

WiFiClientSecure client;
void initWifi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(_WIFI_SSID, _WIFI_PWD);

  Serial.println("Connecting..");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.println(".");
  }
  Serial.printf("SSID: %s\nIP: %s\n", _WIFI_SSID, WiFi.localIP().toString().c_str());
}

///////////////////////////////////////////////////////////////////////////////////////////
//                    QUERY                                                              //
///////////////////////////////////////////////////////////////////////////////////////////

//
// REQUEST FORMAT:
//

/*
{
  query: "...the raw query text...",
  variables: {
    signalId: "...",
    signalSecret: "...",
    _MEASUREMENT: 10
  }
}

reqJson = {
  query: "...",
  variables: varJson
}
*/

// #define SYNCPRINT_SIZE 256
#define REQBUFF_SIZE 256
#define VARBUFF_SIZE 256
#define RESPBUFF_SIZE 2048

const char *_API_HOST = "https://api.chalmersproject.com/graphql";
// Attempting to do a multi-line variable declaration: HOWTO?
const char *MUTATION = "           \
mutation CreateSignalMeasurement(  \
  $signalId: ID!                   \
  $signalSecret: String!           \
  $measurement: Int!               \
) {                                \
  createSignalMeasurement(         \
    input: {                       \
      signalId: $signalId          \
      signalSecret: $signalSecret  \
      measurement: $measurement    \
    }                              \
  ) {                              \
    measurement {                  \
      id                           \
    }                              \
  }                                \
}";

const char *value = "              \
query CheckSignalMeasurement(      \
  $signalId: ID!                   \
) {                                \
    signal(id: $signalId)  {       \
      value                        \
    }                              \
}";

typedef struct graphqlQuery
{
  char req[REQBUFF_SIZE];
  char var[VARBUFF_SIZE];
  int status;
  String resp;
} GraphqlQuery;

// HTTP POST to chalmersproject API
void occupancy_request(WiFiClientSecure client, int occupancy)
{
  // GraphqlQuery *graphql = (GraphqlQuery *)malloc(sizeof(GraphqlQuery));
  HTTPClient http;
  DynamicJsonDocument reqJson(1024);
  DynamicJsonDocument varJson(1024);
  varJson["signalId"] = SIGNAL_ID;
  varJson["signalSecret"] = SIGNAL_SECRET;
  varJson["measurement"] = occupancy;

  Serial.println("Sending HTTP POST");
  http.begin(client, _API_HOST);
  http.addHeader("Content-Type", "application/json");
  reqJson["query"] = MUTATION;
  reqJson["variables"] = varJson;

  String request;
  serializeJson(reqJson, request);
  Serial.print("Request: ");
  Serial.println(request);

  http.POST(request);

  Serial.print("Response: ");
  Serial.println(http.getString());
}
///////////////////////////////////////////////////////////////////////////////////////////
//                    Rotary Encoder Interrupt                                           //
///////////////////////////////////////////////////////////////////////////////////////////
// flag to mark when the dial has been moved. Calls the LEDs, LCD, and push_to_firebase functions
bool change_to_push = false;

void ICACHE_RAM_ATTR encoder_change_trigger()
{
  encoder.tick();
  Serial.println("interrupt triggered!");
}

///////////////////////////////////////////////////////////////////////////////////////////
//                    MAIN SCRIPT STARTS HERE                                            //
///////////////////////////////////////////////////////////////////////////////////////////

void setup()
{
  Serial.begin(115200);
  attachInterrupt(digitalPinToInterrupt(4), encoder_change_trigger, CHANGE);
  attachInterrupt(digitalPinToInterrupt(5), encoder_change_trigger, CHANGE);

  tft.begin();
  tft.setRotation(2);
  tft.setTextColor(WHITE, BLACK);
  tft.setTextSize(2);
  tft.setCursor(0, y2);
  tft.println("CHALMERS");
  tft.setTextSize(3);
  tft.println(" SIGNAL");

  if ( enable_internet == true)
  {
    initWifi();
    // HTTPClient http;
    client.setInsecure();

    if (WiFi.status() == WL_CONNECTED)
    {
      tft.clearScreen(BLACK);
      tft.setTextSize(2);
      tft.setTextSize(2);
      tft.setCursor(0, y2);
      tft.println("   Wi-Fi");
      tft.println(" CONNECTED");
      delay(4000);
    }
    
    // TODO:
    // verify connection to api.chalmersproject.com
    //

  }
  
  // 
  // setup display with occupancy / capacity
  // 
  tft.clearScreen(BLACK);
  tft.setRotation(2);
  tft.setCursor(35, y1);
  tft.setTextColor(WHITE, BLACK);
  tft.setTextSize(5);
  tft.println(occupancy);
  tft.setCursor(8, y2);
  tft.println("----");
  tft.setCursor(35, y3);
  tft.println(capacity);

  //
  // start LED with color of occupancy / capacity
  //
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(led_brightness);

  hue = map(occupancy, 0, capacity, 90, 0);
  CHSV color = CHSV(hue, 255, 255);
  fill_solid(leds, NUM_LEDS, color);
  FastLED.show();

}

unsigned long now, last;

void loop()
{
  static int pos = 0;

  int newPos = encoder.getPosition();
  if (pos != newPos)
  {
    Serial.print("Pos: ");
    Serial.print(newPos);
    Serial.println();
    if (pos > newPos)
    {
      occupancy--;
    }
    else if (newPos > pos)
    {
      occupancy++;
    }
    Serial.print("occupancy: ");
    Serial.print(occupancy);
    Serial.println();
    pos = newPos;

    //set barriers on occupancy
    if (0 >= occupancy)
    {
      occupancy = 0;
    }
    else if (occupancy >= capacity)
    {
      occupancy = capacity;
    }
    tft.setCursor(35, y1);
    // used to detect when occupancy has grown by one digit ( e.g. 10 -> 9 ) and occupancy has to be wiped from the LCD
    if (occupancy == 9 && last_occupancy == 10 || occupancy == 99 && last_occupancy == 100)
    {
      tft.fillRect(35, y1, tft.width(), (y2 - 25), BLACK);
      last_occupancy = occupancy;
    }
    tft.println(occupancy);
    // update LEDs
    hue = map(occupancy, 0, capacity, 90, 0);
    CHSV color = CHSV(hue, 255, 255);
    fill_solid(leds, NUM_LEDS, color);
    FastLED.show();
    change_to_push = true;
    last = now;
    last_occupancy = occupancy;
  }

  //
  // wait at least 3 seconds since last change before pushing to api.chalmers.project
  //
  now = millis();
  if (now - last >= 3000 && change_to_push)
  {
    if (enable_internet == true)
    {
      Serial.println("pushing to api.chalmersproject.com");
      occupancy_request(client, occupancy);
      change_to_push = false;
    }
  }
}
