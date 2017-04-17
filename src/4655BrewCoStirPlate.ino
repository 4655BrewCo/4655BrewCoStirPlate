/*
 *   Copyright(c) 4655 Brewing Company
 *   
 *   This sketch is written for the 4655BrewCoStirPlate, a stir plate to 
 *   cultivate yeast for brewing.

 *   Besides driving a stir plate, it colletcts the room temperature and 
 *   humidity and publishes the values to particle cloud.   
 *
 *   The MIT License(MIT)
 *   
 *   Permission is hereby granted, free of charge, to any person obtaining a copy
 *   of this software and associated documentation files(the "Software"), to deal
 *   in the Software without restriction, including without limitation the rights
 *   to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
 *   copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions :
 *   The above copyright notice and this permission notice shall be included in
 *   all copies or substantial portions of the Software.
 *   
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *   THE SOFTWARE.
 *   
 */

// This #include statement was automatically added by the Particle IDE.
#include <Adafruit_DHT_Particle.h>

// Global variables
double _roomTemperature, _roomHumidity;
int _potValue, _fanSpeed;
bool _fanStatus;
String _stirplateMode;
const int LOOP_DELAY = 500;
const int SENSOR_READ_DELAY = 5000;
const int PUBLISH_DELAY = 60000;
const int FAN_STARTUP_TIME = 5000;
const int FAN_MIN_VALUE = 30;
const int FAN_MAX_VALUE = 255;
const int POT_MIN_VALUE = 50;
const String FAN_MODE_MANUAL = "MANUAL";
const String FAN_MODE_AUTO = "AUTO";
const String FAN_VALUE_TYPE_POT = "POT";
const String FAN_VALUE_TYPE_FAN = "FAN";
unsigned long _lastPublishTime = 0;
unsigned long _lastSensorReadTime = 0;
unsigned long _fanStartTime = 0;
unsigned long _fanRunTime = 0;

//
// DHT setup
//
#define DHT_PIN D4 // what pin we're connected to
                   // what DHT type are we using?
#define DHT_TYPE DHT22 // DHT 22 (AM2302)

// Create an object to talk to the Adafruit_DHT class
DHT dht(DHT_PIN, DHT_TYPE);

//
// POTENTIOMETER setup
//
//   min value: 0
//   med value: ~1695; not connected
//   max value: 3672
#define POT_PIN A0 // what pin we're connected to
//

//
// STIRPLATE setup
//
#define TIP_PIN A7 // what pin we're connected to
//

void setup()
{
  Serial.begin(9600);
  Serial.println("Starting up... ");

  // setup and initialize DHT sensor
  dht.begin();

  // setup and initialize POT value
  _potValue = 0;
  
  // setup and initialize TIP value
  pinMode(TIP_PIN, OUTPUT); // set pin for output to control TIP base pin
  _fanStatus = false;
  analogWrite(TIP_PIN, 0);  // set value to 0, to not immediatly start stirplate
  
  // set stirplate to manual mode
  _stirplateMode = FAN_MODE_MANUAL;

  // setup 
  Particle.variable("potValue", _potValue);
  Particle.variable("roomTemp", _roomTemperature);
  Particle.variable("roomHumidity", _roomHumidity);
  Particle.variable("mode", _stirplateMode);
  Particle.variable("fanSpeed", _fanSpeed);
  Particle.variable("fanStatus", _fanStatus);
  Particle.variable("fanRunTime", _fanRunTime);
  Particle.function("fanMode", funcSetStirplateMode);
  Particle.function("fanSpeed", funcSetFanSpeed);
}

void loop()
{
  unsigned long startMillis = millis();
  int status = 0;
  
  Serial.println("");
  Serial.println("loop...");

  // read POT value
  status = getPotValue();
  // read DHT22 values
  status = getDHT22Values();

  // Set TPM/Fan according to POT value
  Serial.println("stirplateMode [" + _stirplateMode + "]");
  if (_stirplateMode == FAN_MODE_MANUAL)
  {
    Serial.println("stirplateMode [" + _stirplateMode + "], _potValue [" + String(_potValue) + "].");
    status = checkFanStatus(_potValue, FAN_VALUE_TYPE_POT);
  }
  else
  {
    Serial.println("stirplateMode [" + _stirplateMode + "], _fanSpeed [" + String(_fanSpeed) + "].");
    status = checkFanStatus(_fanSpeed, FAN_VALUE_TYPE_FAN);
  }  

  // publish values to particle cloud
  particlePublish();
  
  // send duration to serial
  printDurationInfo(startMillis, millis());

  delay(LOOP_DELAY);
}

/**
* Set fan speed of stir plate through cloud function.
*/
int funcSetFanSpeed(String command)
{
  int status = 0;
  Serial.println("funcStirplateSpeed...");
  Serial.println("... _stirplateMode [" + _stirplateMode);    

  if (_stirplateMode == FAN_MODE_AUTO)
  {
    int fanSpeed = command.toInt();
    Serial.println("... fanSpeed [" + String(fanSpeed) + "]");
    status = checkFanStatus(fanSpeed, FAN_VALUE_TYPE_POT);
  }
  else
  {
    Serial.println("... error");
    status = 1;
  }

  return status;
}

/**
* Set mode of stirplate through cloud function.
*/
int funcSetStirplateMode(String command)
{
  int status = 0;
  Serial.println("funcStirplateMode...");
  String mode = command.toUpperCase();
  Serial.println("... mode ["+ mode +"]");  
  
  Serial.println("... set _stirplateMode [" + mode + "]");
  if (mode == FAN_MODE_MANUAL || mode == FAN_MODE_AUTO)
  {
    _stirplateMode = mode;
  }
  else 
  {
    Serial.println("... no valid mode [" + mode + "]");
    status = 1;
  }

  return status;
}

/**
* Publish values to particle
*
* @param none
* @return  0 - success
*          1 - error
*         99 - no read required yet
*/
int particlePublish()
{
  int status = 0;

  if (millis() - _lastPublishTime > PUBLISH_DELAY)
  {
    _lastPublishTime = millis();
    String jsonDataString = String("{ \"roomTemperature\":" + String(_roomTemperature) + ",\"roomHumidity\":" + String(_roomHumidity) + ",\"stirplateMode\": \"" + _stirplateMode + "\",\"potValue\":" + String(_potValue) + ",\"fanStatus\":" + String(_fanStatus) + ",\"fanSpeed\":" + String(_fanSpeed) + ",\"fanRunTime\":" + String(_fanRunTime) + "}");
    Particle.publish("stirplate-metric", jsonDataString);
    Particle.publish("room/temperature", String(_roomTemperature), PRIVATE);
    Particle.publish("room/humidity", String(_roomHumidity), PRIVATE);
    Particle.publish("fan/status", String(_fanStatus), PRIVATE);
  }
  else
  {
    status = 99;
  }

  return status;
}

/**
* Check and set fan speed based on fanSpeed value.
*
* @param   value - value
*          valueType - POT or FAN
* @return  0 - success
*          1 - error
*/
int checkFanStatus(int value, String valueType)
{ 
  int status = 0;
  Serial.println("Check fan status...");

  int fanSpeed = value;
  
  if (valueType == FAN_VALUE_TYPE_POT)
  {
    if (value < POT_MIN_VALUE)
    {
      fanSpeed = 0;
    }
    else
    {
      fanSpeed = map(value, POT_MIN_VALUE, 3672, FAN_MIN_VALUE, FAN_MAX_VALUE);
    }
  }

  if (fanSpeed < FAN_MIN_VALUE)
  {
    if (_fanStatus == true)
    {
      Serial.println("... fanSpeed [" + String(fanSpeed) + "] < " + String(FAN_MIN_VALUE) + " - turn fan off!");
      fanOff();
    }
    else
    {
      Serial.println("... fanSpeed [" + String(fanSpeed) + "] < " + String(FAN_MIN_VALUE) + " - keep fan off!");
      fanOff();
    }
  }
  else
  {
    if (_fanStatus == false)
    {
      Serial.println("... fanSpeed [" + String(fanSpeed) + "] > " + String(FAN_MIN_VALUE) + " - turn fan on!");
      fanStartup(fanSpeed);
    }
    else
    {
      if (millis() - _fanStartTime < FAN_STARTUP_TIME)
      {
        Serial.println("... keep spinning fan on!");
      }
      else if (_fanSpeed == fanSpeed)
      {
        Serial.println("... keep fan speed - fanSpeed [" + String(fanSpeed) + "]");
      }
      else
      {
        Serial.println("... change fan speed! fanSpeed [" + String(fanSpeed) + "]");
        setFanSpeed(fanSpeed);
      }
    }
  }

  return status;
}

/**
* Get temperature and humidity value from DHT22 sensor.
*
* @param none
* @return  0 - success
*          1 - error
*         99 - no read required yet
*/
int getDHT22Values()
{
  int status = 0;

  // read sensor values every SENSOR_READ_DELAY.
  if (millis() - _lastSensorReadTime > SENSOR_READ_DELAY)
  {
    _lastSensorReadTime = millis();
    // read DHT temperature
    float roomTemperature = dht.getTempCelcius();
    // read DHT humidity
    float roomHumidity = dht.getHumidity();

    // check if any reads failed
    if (isnan(roomTemperature) || isnan(roomHumidity))
    {
      Serial.println("Failed to read from DHT sensor!");
      status = 1;
    }
    else
    {
      // set public variables
      _roomTemperature = roomTemperature;
      _roomHumidity = roomHumidity;
      // send to serial
      printTemperatureInfo(roomTemperature, roomHumidity);

      status = 0;
    }
  }
  else
  {
    // set status to 99
    status = 99;
  }

  return status;
}

/**
* Get pot value from potentiometer.
*
* @param none
* @return  0 - success
*          1 - error
*/
int getPotValue()
{
  int status = 0;

  // read POT value
  int potValue = analogRead(POT_PIN);

  if (isnan(potValue))
  {
    status = 1;
    Serial.println("Failed to read from POT sensor!");
  }
  else
  {
    status = 0;
    // set public variables
    _potValue = potValue;
    // send to serial
    printPotInfo(potValue);
  }

  return status;
}

void doSetFanSpeed(int fanSpeed)
{
  analogWrite(TIP_PIN, fanSpeed);
}

void setFanSpeed(int fanSpeed)
{
  // set fan variables to OFF
  if (fanSpeed == 0)
  {
    _fanStatus = false;
    _fanStartTime = 0;
  }
  // set fan speed with value provided
  _fanSpeed = fanSpeed;
  // do change actual fan speeds
  doSetFanSpeed(fanSpeed);
}

void fanStartup(int fanSpeed)
{
  // set fan variables to ON
  _fanStartTime = millis();
  _fanStatus = true;
  _fanSpeed = fanSpeed;
  // set fan speed
  doSetFanSpeed(FAN_MAX_VALUE);
}

void fanOff()
{
  // set fan speed
  setFanSpeed(0);
}

void printTemperatureInfo(float roomTemperature, float roomHumidity)
{
  // send to serial
  Serial.println("room temperature and humidity:");
  Serial.print("  room temp [");
  Serial.print(roomTemperature);
  Serial.println(" *C]");
  Serial.print("  humidity  [");
  Serial.print(roomHumidity);
  Serial.println(" %]");
}

void printPotInfo(int potValue)
{
  // send to serial
  Serial.println("potentiometer value:");
  Serial.print("  pot value [");
  Serial.print(potValue);
  Serial.println("]");
}

void printDurationInfo(long startMillis, long endMillis)
{
  unsigned long durationMillis = endMillis - startMillis;
  Serial.print("ran for ");
  Serial.print(durationMillis);
  Serial.print("[ms] - ");
  Serial.print("sleep for ");
  Serial.print(LOOP_DELAY);
  Serial.println("[ms]...");
}