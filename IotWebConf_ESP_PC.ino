/*
 * Contributors:  Christian Richardson Quiday Escobia
 *                Le Thanh Tung
 *                Duong Thanh Minh
 *                Nguyen Tran Minh Luan
 *
 * Description: 
 *      This project demonstrates how WiFi connection settings 
 *      can be managed through a Captive Portal provided by 
 *      the IotWebConf library. One major highlight is that the 
 *      "Sign in to network" automatically pops up in the 
 *      browser of the connected device: mobile/laptop/computer. 
 *      Moreover, the sensor data from the Thing is displayed 
 *      on the webpage.
 *
 * Default name of the thing: testThing //Initial Name of the Thing, where it is used as the SSID of its own Access Point
 * Default password: smrtTHNG8266 // Initial password to connect to the Thing, when it creates its own Access Point
 */

#include "Button2.h"      // Lib for OLED display
#include "SSD1306Wire.h"  // Lib for environment sensor BME280
#include <Adafruit_BME280.h>    // Lib for handling button events
#include <IotWebConf.h>   // Lib for web configuration portal

SSD1306Wire  display(0x3c,5,4); // create an SSD1306Wire object with name 
                                // "display" with I2C at the pins 5 and 4 and
                                // I2C adress 0x3c

Adafruit_BME280 bme; // create BM280 object using I2C with name "bme"

// Web Config Portal
const char thingName[] = "IotWebConfServer";
const char wifiInitialApPassword[] = "123456789";

#define STRING_LEN 128
#define NUMBER_LEN 32

// Configuration specific key. The value should be modified if config structure was changed.
#define CONFIG_VERSION "dem1"

// When CONFIG_PIN is pulled to ground on startup, the Thing will use the initial
// password to build an AP. (E.g. in case of lost password)
#define CONFIG_PIN 14

// -- Status indicator pin.
//      First it will light up (kept LOW), on Wifi connection it will blink,
//      when connected to the Wifi it will turn off (kept HIGH).  
#define STATUS_PIN LED_BUILTIN

// -- Callback method declarations.
void configSaved();
boolean formValidator();
DNSServer dnsServer;
WebServer server(80);

char stringParamValue[STRING_LEN];
char intParamValue[NUMBER_LEN];
char floatParamValue[NUMBER_LEN];

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);
IotWebConfParameter stringParam = IotWebConfParameter("String param", "stringParam", stringParamValue, STRING_LEN);
IotWebConfSeparator separator1 = IotWebConfSeparator();
IotWebConfParameter intParam = IotWebConfParameter("Int param", "intParam", intParamValue, NUMBER_LEN, "number", "1..100", NULL, "min='1' max='100' step='1'");
// -- We can add a legend to the separator
IotWebConfSeparator separator2 = IotWebConfSeparator("Calibration factor");
IotWebConfParameter floatParam = IotWebConfParameter("Float param", "floatParam", floatParamValue, NUMBER_LEN, "number", "e.g. 23.4", NULL, "step='0.1'");

unsigned long intervals[3][2] = {{1000,0},{60000,0},{5000,0}}; /* {{1s interval,prev_millis},{1s interval,prev_millis},{1min interval,prev_millis}} */
unsigned long current_millis = 0;

float temperature = 0.0;
float pressure = 0.0;
float altitude = 0.0;
float humidity = 0.0;
bool movement = 0;
String buttons[4] = {"0","0","0","0"};

bool values_changed = 0;  // refresh display only if values changed

Button2 button_1 = Button2(12,INPUT_PULLUP,false,50);   // create the butto objects for the 4 buttons
Button2 button_2 = Button2(13,INPUT_PULLUP,false,50);
Button2 button_3 = Button2(14,INPUT_PULLUP,false,50);
Button2 button_4 = Button2(15,INPUT_PULLUP,false,50);

void setup() {
    Serial.begin(115200);  // starts serial monitor

    display.connect(); // connects the display and sets the pins of I2C according the definition above
                       // this is necessary to overwrite the defaults for bme object
    
    bool status;
    status = bme.begin(0x76);  // starts connection to BME280 at I2C adress 0x76
    if (!status) {
        Serial.println("Could not find a valid BME280 sensor, check wiring, address, sensor ID!");
    }
    
    display.init();           // display initialisation
    display.setContrast(100);
    display.clear();
    display.setFont(ArialMT_Plain_10);

    button_1.setClickHandler(handle_button_1);        // assign the callback funktions for the different button events
    button_1.setDoubleClickHandler(handle_button_1);
    button_1.setLongClickHandler(handle_button_1);
    button_2.setClickHandler(handle_button_2);
    button_2.setDoubleClickHandler(handle_button_2);
    button_2.setLongClickHandler(handle_button_2);
    button_3.setClickHandler(handle_button_3);
    button_3.setDoubleClickHandler(handle_button_3);
    button_3.setLongClickHandler(handle_button_3);
    button_4.setClickHandler(handle_button_4);
    button_4.setDoubleClickHandler(handle_button_4);
    button_4.setLongClickHandler(handle_button_4);

    readBME280();

    iotWebConf.setStatusPin(STATUS_PIN);
    iotWebConf.setConfigPin(CONFIG_PIN);
    iotWebConf.addParameter(&stringParam);
    iotWebConf.addParameter(&separator1);
    iotWebConf.addParameter(&intParam);
    iotWebConf.addParameter(&separator2);
    iotWebConf.addParameter(&floatParam);
    iotWebConf.setConfigSavedCallback(&configSaved);
    iotWebConf.setFormValidator(&formValidator);
    iotWebConf.getApTimeoutParameter()->visible = true;
    iotWebConf.init();
    server.on("/", handleRoot);
    server.on("/config", []{ iotWebConf.handleConfig(); });
    server.onNotFound([](){ iotWebConf.handleNotFound(); });
}


void loop() { 
    current_millis = millis();
    if(current_millis - intervals[0][1] > intervals[0][0]){ // 1s interval for PIR sensor
      readPIR();
      intervals[0][1] = current_millis;
      }  
    if(current_millis - intervals[1][1] > intervals[1][0]){ // 1min interval for BME280
      readBME280();
      intervals[1][1] = current_millis;
      }
    if(current_millis - intervals[2][1] > intervals[2][0]){ // 5s interval for button reset
      buttons[0] = buttons[1] = buttons[2] = buttons[3] = "0";
      values_changed = 1;
      intervals[2][1] = current_millis;
      }
    if(values_changed){
      printValues();
      values_changed = 0;
      }
    button_1.loop();
    button_2.loop();
    button_3.loop();
    button_4.loop();

    iotWebConf.doLoop();
}

void handle_button_1(Button2& btn){   // callback functions that are called if the corrospondign button is clicked
  buttons[0] = getClicks(btn);
  values_changed = 1;
  }
void handle_button_2(Button2& btn){
  buttons[1] = getClicks(btn);
  values_changed = 1;
  }
void handle_button_3(Button2& btn){
  buttons[2] = getClicks(btn);
  values_changed = 1;
  }
void handle_button_4(Button2& btn){
  buttons[3] = getClicks(btn);
  values_changed = 1;
  }
  
String getClicks(Button2& btn){   // auxilliary function to find out the button event
  String c = "0";
  switch(btn.getClickType()){
    case SINGLE_CLICK:
      c = "S";
      break;
    case DOUBLE_CLICK:
      c = "D";
      break;
    case LONG_CLICK:
      c = "L";
      break;
  }
  return(c);
}

void readBME280(){
  float tmp = bme.readTemperature();
  if(temperature != tmp){
    temperature = tmp;
    values_changed = 1;
  }
  tmp = bme.readPressure() / 100.00;
  if(pressure != tmp){
    pressure = tmp;
    values_changed = 1;
  }
  tmp = bme.readAltitude(1013.25); //1013.25 = sea level pressure
  if(altitude != tmp){
    altitude = tmp;
    values_changed = 1;
  }
  tmp = bme.readHumidity();
  if(humidity != tmp){
    humidity = tmp;
    values_changed = 1;
  }
}

void readPIR(){
  float tmp = digitalRead(16);
  if(movement != tmp){
    movement = tmp;
    values_changed = 1;
    }
}

void printValues() {
    String T = String("Temperature = " + (String)temperature + " °C");
    String P = String("Pressure = " + (String)pressure + " hPa");
    String A = String("Approx. Altitude = " + (String)altitude + " m");
    String H = String("Humidity = " + (String)humidity + " %");
    String M = String("Movement = " + (String)movement);
    String B = String("Buttons = " + buttons[0] + " " + buttons[1] + " " + buttons[2] + " " + buttons[3]);
    
    Serial.println(T);            // prints values to the serial monitor
    Serial.println(P);
    Serial.println(A);
    Serial.println(H);
    Serial.println(M);
    Serial.println(B);
    Serial.println();

    String all = T + P + A + H + M + B;
    display.clear();

    display.drawString(0,0,T);    // writes values to the OLED memory
    display.drawString(0,10,P);
    display.drawString(0,20,A);
    display.drawString(0,30,H);
    display.drawString(0,40,M);
    display.drawString(0,50,B);
    
    display.display();            // displays the content of the OLED memory
}

//wifi
void handleRoot()
{
  // -- Let IotWebConf test and handle captive portal requests.
  if (iotWebConf.handleCaptivePortal())
  {
    // -- Captive portal request were already served.
    return;
  }
  String s = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
  s += "<title>IotWebConf 03 Custom Parameters</title></head><body>Hello world!";
  s += "<ul>";
  s += "<li>String param value: ";
  s += stringParamValue;
  s += "<li>Int param value: ";
  s += atoi(intParamValue);
  s += "<li>Float param value: ";
  s += atof(floatParamValue);
  s += "<li>Temperature: ";
  s += (String)temperature + " C";
  s += "<li>Pressure: ";
  s += (String)pressure + " hPa";
  s += "<li>Approx. Altitude: ";
  s += (String)altitude + " m";
  s += "<li>Humidity: ";
  s += (String)humidity + " %";
  s += "<li>Movement: ";
  s += (String)movement;
  s += "<li>Buttons: ";
  s += buttons[0] + " " + buttons[1] + " " + buttons[2] + " " + buttons[3];
  s += "</ul>";
  s += "Go to <a href='config'>configure page</a> to change values.";
  s += "</body></html>\n";

  server.send(200, "text/html", s);
}

void configSaved()
{
  Serial.println("Configuration was updated.");
}

boolean formValidator()
{
  Serial.println("Validating form.");
  boolean valid = true;

  int l = server.arg(stringParam.getId()).length();
  if (l < 3)
  {
    stringParam.errorMessage = "Please provide at least 3 characters for this test!";
    valid = false;
  }

  return valid;
}
