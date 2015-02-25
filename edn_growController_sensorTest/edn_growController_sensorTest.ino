#include <OneWire.h>
#include <DallasTemperature.h>
#include "DHT.h"

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

// Water temperature sensor - Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);  

// Water temperature sensor - Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire); 

// Water temperature sensor - arrays to hold device address
DeviceAddress insideThermometer;
    
// Temperature & Humidity sensor - Initialize DHT sensor for normal 16mhz Arduino
DHT dht(DHTPIN, DHT11);

void setup(void)
{
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
  
}
  
void loop(void)
{

  checkSensors();  
  
  // Wait 2 seconds
  delay(5000);
  
}

void checkSensors() {
  
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
    
  // Print light measurement
  Serial.print("Light level: ");
  Serial.println(lightLevel);
  
  // Print temperature and humidity measurements
  Serial.print("Humidity: "); 
  Serial.print(humidity);
  Serial.print(" %\t");
  Serial.print("Temperature: "); 
  Serial.print(temperatureC);
  Serial.print(" *C ");
  Serial.print(temperatureF);
  
  // Print CO2 measurements
  Serial.print(" *F\n");
  Serial.print( "SEN-00007:" );
  Serial.print(volts); 
  Serial.print( "V           " );
  Serial.print("CO2:");
  if (CO2percentage == -1) {
      Serial.print( "<400" );
  } else {
      Serial.print(CO2percentage);
  }
  Serial.print( "ppm" );  
  Serial.print( "       Time point:" );
  Serial.print(millis());
  Serial.print("\n");
  
  // Print water temperature measurements
    Serial.print("Water Temperature *C: ");
  Serial.print(waterTempC);
  Serial.print("\t Water Temperature *F: ");
  Serial.print(waterTempF);
  
  Serial.print("\n\n");
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
      return -1;
   } else { 
      return pow(10, ((volts/DC_GAIN)-pcurve[1])/pcurve[2]+pcurve[0]);
   }
}

