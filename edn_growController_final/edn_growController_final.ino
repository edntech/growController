
// if your plant is in the vegetative growth stage then this variable should be "veg"
// if your plant is in the flowering growth stage then this variable should be "flower"
String growthStage = "flower";  

#include <Bridge.h>
#include <Temboo.h>
#include <Process.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Time.h>
#include <TimeAlarms.h>
#include "DHT.h"
#include "TembooAccount.h" // contains Temboo account information
#include "googleAccount.h" // containes Google account information

// Light and water pump pins
#define WATERPUMP 7
#define LED 8

// temperature & humidity sensor pin
#define DHTPIN 4 

// CO2 sensor pins and contstants
#define MG_PIN (1)     // analog pin for CO2 data
#define BOOL_PIN (0)
#define DC_GAIN (8.5)   // define the DC gain of amplifier
#define READ_SAMPLE_INTERVAL (50)    //define how many samples you are going to take in normal operation
#define READ_SAMPLE_TIMES (5)     //define the time interval(in milisecond) between each samples in normal operation
//These two values differ from sensor to sensor. user should derermine this value.
#define ZERO_POINT_VOLTAGE (0.220) //define the output of the sensor in volts when the concentration of CO2 is 400PPM
#define REACTION_VOLTGAE (0.020) //define the voltage drop of the sensor when move the sensor from air into 1000ppm CO2

// Water temperature sensor
#define ONE_WIRE_BUS 2

// Variables
unsigned long time;
int lightLevel;
float humidity;
float temperatureC;
float temperatureF;
float CO2Curve[3] = {2.602,ZERO_POINT_VOLTAGE,(REACTION_VOLTGAE/(2.602-3))};   
      //two points are taken from the curve.  With these two points, a line is formed which is
      //"approximately equivalent" to the original curve.
      //data format:{ x, y, slope}; point1: (lg400, 0.324), point2: (lg4000, 0.280) 
      //slope = ( reaction voltage ) / (log400 –log1000) 
int CO2percentage; // CO2 sensor reading
float volts; // CO2 sensor reading
float waterTempC;
float waterTempF;
int hours, minutes, seconds;  // used to set the time
int lastSecond = -1;          // need an impossible value for comparison 
String filename; // variable for webcam picture filename
String path = "/mnt/sda1/";  // path that webcam pictures will be saved to
int night;  // the hour of the day when the lights will turn off
unsigned long startTime, currentTime;  // used to track how long the water pump has been on for

// Water temperature sensor - Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);  

// Water temperature sensor - Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire); 

// Water temperature sensor - arrays to hold device address
DeviceAddress insideThermometer;

// Process to get the time measurement
Process date;

// Process to take pictures with the webcam
Process picture;
    
// Temperature & Humidity sensor - Initialize DHT sensor for normal 16mhz Arduino
DHT dht(DHTPIN, DHT11);

void setup(void) {
  // Init serial
  Serial.begin(9600);
  
  // Configure the temperature and humidity pin
  dht.begin();
  
  // Configure CO2 pins
  pinMode(BOOL_PIN, INPUT); //set pin to input
  digitalWrite(BOOL_PIN, HIGH); //turn on pullup resistors
  
  // Water temperature sensor - locate devices on the bus
  sensors.begin();
  if (!sensors.getAddress(insideThermometer, 0)) Serial.println("Unable to find address for Device 0"); 
  // set the resolution to 9 bit (Each Dallas/Maxim device is capable of several different resolutions)
  sensors.setResolution(insideThermometer, 9);
  
  // Start bridge
  Bridge.begin();
  
    // Start date process
  time = millis();
  if (!date.running())  {
    date.begin("date");
    date.addParameter("+%D-%T");
    date.run();
  }
  
  // Check whether to use a vegatative or flowering lighting schedule
  if(growthStage == "flower") {
    night = 18;
  } else if (growthStage == "veg") {
    night = 0;
  }
  
  // Check the time using the local wifi network that your Yun is connected to
  checkTime();
  
  // Set the time
  setTime(hours,minutes,seconds, 1,1,11);
  
  // Set timers and alarms
  Alarm.timerRepeat(10800,runAppendRow); // check sensors every 3 hours and send data to the google doc
  Alarm.alarmRepeat(7,0,0, takePicture); // take a picture at 7am and save to SD card
  Alarm.alarmRepeat(17,30,0, takePicture); // take a picture at 5:30pm and save to SD card
  Alarm.alarmRepeat(6,0,0, light_on); // turn the lights on at 6am
  Alarm.alarmRepeat(night,0,0, light_off); // turn the lights off
  Alarm.timerRepeat(600, waterPlant); // (Frequency, Function)
  
  waterPlant();
  runAppendRow();
  
}

  
void loop(void)
{
   Alarm.delay(1000);
}

String checkSensors() {
  
  // Measure light level
  lightLevel = analogRead(A0);
  
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  humidity = dht.readHumidity();
  // Read temperature as Celsius
  temperatureC = dht.readTemperature();
  // Read temperature as Fahrenheit
  temperatureF = dht.readTemperature(true);

  // CO2 sensor readings
  volts = MGRead(MG_PIN);
  CO2percentage = MGGetPercentage(volts,CO2Curve);

  // Water temperature sensor reading
  sensors.requestTemperatures(); // Send the command to get temperatures
  waterTempC = sensors.getTempC(insideThermometer);
  waterTempF = DallasTemperature::toFahrenheit(waterTempC);
    
  String dataString = String(lightLevel) + "," + String(humidity) + "," + String(temperatureC) + "," + String(temperatureF) + "," + String(CO2percentage) + "," + String(waterTempC) + "," + String(waterTempF);
  
  return dataString;
}

/*****************************  MGRead *********************************************
Input:   mg_pin - analog channel
Output:  output of SEN-000007
Remarks: This function reads the output of SEN-000007
************************************************************************************/ 
float MGRead(int mg_pin)
{
    int i;
    float v=0;

    for (i=0;i<READ_SAMPLE_TIMES;i++) {
        v += analogRead(mg_pin);
        delay(READ_SAMPLE_INTERVAL);
    }
    v = (v/READ_SAMPLE_TIMES) *5/1024 ;
    return v;  
}

/*****************************  MQGetPercentage **********************************
Input:   volts   - SEN-000007 output measured in volts
         pcurve  - pointer to the curve of the target gas
Output:  ppm of the target gas
Remarks: By using the slope and a point of the line. The x(logarithmic value of ppm) 
         of the line could be derived if y(MG-811 output) is provided. As it is a 
         logarithmic coordinate, power of 10 is used to convert the result to non-logarithmic 
         value.
************************************************************************************/ 
int  MGGetPercentage(float volts, float *pcurve)
{
   if ((volts/DC_GAIN )>=ZERO_POINT_VOLTAGE) {
      return 400;
   } else { 
      return pow(10, ((volts/DC_GAIN)-pcurve[1])/pcurve[2]+pcurve[0]);
   }
}

void runAppendRow() {
  
  String sensorValues = checkSensors();
  
  // we need a Process object to send a Choreo request to Temboo
  TembooChoreo AppendRowChoreo;
  
  // invoke the Temboo client
  // NOTE that the client must be reinvoked and repopulated with
  // appropriate arguments each time its run() method is called.
  AppendRowChoreo.begin();
  
  // set Temboo account credentials
  AppendRowChoreo.setAccountName(TEMBOO_ACCOUNT);
  AppendRowChoreo.setAppKeyName(TEMBOO_APP_KEY_NAME);
  AppendRowChoreo.setAppKey(TEMBOO_APP_KEY);
  
  // identify the Temboo Library choreo to run (Google > Spreadsheets > AppendRow)
  AppendRowChoreo.setChoreo("/Library/Google/Spreadsheets/AppendRow");
  
  // your Google username (usually your email address)
  AppendRowChoreo.addInput("Username", GOOGLE_USERNAME);
  
  // your Google account password
  AppendRowChoreo.addInput("Password", GOOGLE_PASSWORD);  
  
  // the title of the spreadsheet you want to append to
  AppendRowChoreo.addInput("SpreadsheetTitle", SPREADSHEET_TITLE);  
  
  // Restart the date process:
  if (!date.running())  {
    date.begin("date");
    date.addParameter("+%D-%T");
    date.run();
  }  

  // convert the time and sensor values to a comma separated string
  String timeString = date.readString();
  Serial.print(timeString);
  String rowData = "";
  rowData = rowData + timeString + "," + sensorValues;

  // add the RowData input item
  AppendRowChoreo.addInput("RowData", rowData); 
  
  // run the Choreo and wait for the results
  // The return code (returnCode) will indicate success or failure
  unsigned int returnCode = AppendRowChoreo.run();  
  
  // return code of zero (0) means success
  if (returnCode == 0) {
    Serial.println("Success! Appended " + rowData);
    Serial.println("");
  } else {
    // return code of anything other than zero means failure
    // read and display any error messages
    while (AppendRowChoreo.available()) {
      char c = AppendRowChoreo.read();
      Serial.print(c);
    }
  }

  AppendRowChoreo.close(); 
}


void checkTime() {
 if(lastSecond != seconds) {  // if a second has passed
    // restart the date process:
    if (!date.running())  {
      date.begin("date");
      date.addParameter("+%T");
      date.run();
    }
  }

  //if there's a result from the date process, parse it:
  while (date.available()>0) {
    // get the result of the date process (should be hh:mm:ss):
    String timeString = date.readString();    

    // find the colons:
    int firstColon = timeString.indexOf(":");
    int secondColon= timeString.lastIndexOf(":");

    // get the substrings for hour, minute second:
    String hourString = timeString.substring(0, firstColon); 
    String minString = timeString.substring(firstColon+1, secondColon);
    String secString = timeString.substring(secondColon+1);

    // convert to ints,saving the previous second:
    hours = hourString.toInt();
    minutes = minString.toInt();
    lastSecond = seconds;          // save to do a time comparison
    seconds = secString.toInt();
  }  
}

void takePicture() {
    // Generate filename with timestamp
    filename = "";
    picture.runShellCommand("date +%s");
    while(picture.running());

    while (picture.available()>0) {
      char c = picture.read();
      filename += c;
    } 
    filename.trim();
    filename += ".png";
 
    // Take picture
    picture.runShellCommand("fswebcam " + path + filename + " -r 1280x720");
    while(picture.running());
}

void waterPlant() {

  // Turn on water pump
  digitalWrite(WATERPUMP, HIGH);

  // Wait 30 seconds
  startTime = millis();
  currentTime = millis();
  while (currentTime - startTime < 30000) {
    currentTime = millis();
  }

  // Turn off water pump
  digitalWrite(WATERPUMP, LOW);
}


void light_on() {
  digitalWrite(LED, HIGH);
}


void light_off() {
  digitalWrite(LED, LOW);
}

