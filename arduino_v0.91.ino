//
// Based on:
// - http://automacile.fr/ds18b20-capteur-de-temperature-one-wire-arduino/
// - http://www.pjrc.com/teensy/td_libs_OneWire.html
// - http://playground.arduino.cc/Learning/OneWire
// - https://datasheets.maximintegrated.com/en/ds/DS18B20.pdf
//
// - http://www.pcbheaven.com/wikipages/How_PC_Fans_Work/
//
// - https://arduino-info.wikispaces.com/LCD-Blue-I2C
// - https://bitbucket.org/fmalpartida/new-liquidcrystal/wiki/Home
//
// - http://eskimon.fr/2498-arduino-annexes-g-utiliser-module-bluetooth-hc-05
// - http://knowledge.parcours-performance.com/arduino-bluetooth-hc-05-hc-06/
//
// - http://www.doctormonk.com/2012/01/arduino-timer-library.html
//
// - https://www.arduino.cc/en/Reference/HomePage
//

#include <Wire.h> // I2C protocol
#include <OneWire.h> // OneWire protocol
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <Timer.h>

// ---------- Initialisation des variables ---------------------

// For 'interruption'
Timer t;

// DS18B20
const int DS18B20_1_PIN = 8;
const int DS18B20_ID = 0x28;

// temperature sensor address
int tempSensor1AddrAquired = 0;
byte tempSensor1Addr[8];

// Fan Relay
const int FANRELAY1_PIN = 9;
//const int FANRELAY2_PIN = 10;

const int OFF = 0;
const int ON  = 1;
volatile int fanModeOrder = OFF; // volatile car modifiable dans loop et lors d'une interruption

const int byTempSensor = 0;
const int byUser       = 1;
volatile int fanMode = byTempSensor; // volatile car modifiable dans loop et lors d'une interruption

// LCD device
const int LCD_ID = 0x3F;

// set the LCD address to 0x3F for a 20 chars 4 line display
// Set the pins on the I2C chip used for LCD connections:
//                    addr, en,rw,rs,d4,d5,d6,d7,bl,blpol
LiquidCrystal_I2C lcd(LCD_ID, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

// Voie 'série' par défaut (affichage console PC - debugging - Pins 0 et 1)
const int SERIAL_PORT_SPEED = 9600; // 9600 bauds

// Voie série pour module bluetooth
const int SERIAL_RX_PIN = 11;
const int SERIAL_TX_PIN = 12;
SoftwareSerial blueToothSerial(SERIAL_RX_PIN, SERIAL_TX_PIN);

// Temperature
const float temperatureLimit = 27; // en degré celsius
float temperature = -1;
float temperatureMin = -1;
float temperatureMax = -1;

void setup() {
  // Initialisation du port de communication avec le PC
  Serial.begin(SERIAL_PORT_SPEED);
  Serial.println("Initialisation du soft");

  blueToothSerial.begin(SERIAL_PORT_SPEED);
  blueToothSerial.println("Bluetooth On...Press:");
  blueToothSerial.println("- 0 to deactivate the fan");
  blueToothSerial.println("- 1 to activate the fan.");
  blueToothSerial.println("- 2 to let it driven through temperature sensor.");
  blueToothSerial.println("- 3 to data temperature detailed information.");

  pinMode(FANRELAY1_PIN, OUTPUT);
  //pinMode(FANRELAY2_PIN, OUTPUT);

  lcd_setup();

  t.every(500, onBluetoothCmdReceived); // every 500ms, check bluetooth serial...
}

void loop() {
  OneWire ds(DS18B20_1_PIN); // on pin DS18B20_1_PIN

  if(!tempSensor1AddrAquired)
    tempSensor1AddrAquired = getTemperatureSensorAddress(ds);

  if(tempSensor1AddrAquired) {
    temperature = getTemperature(ds, tempSensor1Addr); // On lance la fonction d'acquisition de T°
    if (temperature != -1) {

      if(temperatureMin == -1)
        temperatureMin = temperature;
      else if(temperature < temperatureMin)
        temperatureMin = temperature;

      if(temperatureMax == -1)
        temperatureMax = temperature;
      else if(temperature > temperatureMax)
        temperatureMax = temperature;

      lcd_display_temp(temperature, temperatureMin, temperatureMax);

      // Console: T courante, limite, min et max
      Serial.print("T courante = ");
      Serial.println(temperature);
      Serial.print("T limite = ");
      Serial.println(temperatureLimit);
      Serial.print("T min = ");
      Serial.println(temperatureMin);
      Serial.print("T max = ");
      Serial.println(temperatureMax);

      if(fanMode == byTempSensor) {
        if (temperature > temperatureLimit)
          fanModeOrder = ON;
        else
          fanModeOrder = OFF;
      }
    }

    // Finally: drive the Fan(s)
    driveFan(fanModeOrder);
    lcd_display_fanMode(fanModeOrder);

    t.update();

    Serial.println("------------------------------");  
  }
}

void onBluetoothCmdReceived() {
  bool cmdHandled = false;

  if(blueToothSerial.available()) {
    int rxdata = blueToothSerial.read();
    if(rxdata == '1') {
      fanMode = byUser;
      fanModeOrder = ON;
      blueToothSerial.println("FAN mode forced to ON.");
      cmdHandled = true;
    } else if (rxdata == '0') {
      fanMode = byUser;
      fanModeOrder = OFF;
      blueToothSerial.println("FAN mode forced to OFF.");
      cmdHandled = true;
    } else if (rxdata == '2') {
      fanMode = byTempSensor;
      blueToothSerial.println("FAN mode driven by temperature sensor.");
      cmdHandled = true;
    } else if (rxdata == '3') {
      blueToothSerial.println("Temperature detailed information sent.");
      cmdHandled = true;
    }

    if(cmdHandled)
      sendTempThroughBluetooth();
  }
}

void sendTempThroughBluetooth() {
  // Send temperature values & fan status
  blueToothSerial.print("{ \"temp\":\"");
  blueToothSerial.print(temperature);
  blueToothSerial.print("\", \"tempLimit\":\"");
  blueToothSerial.print(temperatureLimit);
  blueToothSerial.print("\", \"tempMin\":\"");
  blueToothSerial.print(temperatureMin);
  blueToothSerial.print("\", \"tempMax\":\"");
  blueToothSerial.print(temperatureMax);
  blueToothSerial.print("\", \"fanStatus\":\"");
  if(fanModeOrder == ON)
    blueToothSerial.print("ON");
  else
    blueToothSerial.print("OFF");
  blueToothSerial.print("\", \"fanMode\":\"");
  if(fanMode == byTempSensor)
    blueToothSerial.print("Sensor");
  else
    blueToothSerial.print("User");
  blueToothSerial.println("\"}");
}


/* --------------- Acquisition de la température ----------------------------------- */

int getTemperatureSensorAddress(OneWire &ids) {
  if ( !ids.search(tempSensor1Addr)) {
    Serial.println("No more addresses.\n");
    ids.reset_search();

    delay(250);
    return 0;
  }

  // Cette fonction sert à surveiller si la transmission s'est bien passée
  if (OneWire::crc8(tempSensor1Addr, 7) != tempSensor1Addr[7]) {
    Serial.println("getTemperatureDS18b20 : <!> CRC is not valid! <!>");
    return 0;
  }

  Serial.print("temperature sensor address = ");
  for ( int i = 0; i < 8; i++) {
    Serial.print(tempSensor1Addr[i], HEX);
    Serial.print(" ");
  }
  Serial.println("");

  // On vérifie que l'élément trouvé est bien un DS18B20
  if (tempSensor1Addr[0] != DS18B20_ID) {
    Serial.println("L'équipement trouvé n'est pas un DS18B20");
    return 0;
  }

  return 1;
}

float getTemperature(OneWire &ids, byte* iSensorAddr) {
  byte i;
  byte data[12];
  float temp = 0.0;

  // Demander au capteur de mémoriser la température et lui laisser 850ms pour le faire (voir datasheet)
  ids.reset();
  ids.select(iSensorAddr);
  ids.write(0x44);

  delay(850);

  // Demander au capteur de nous envoyer la température mémorisé
  ids.reset();
  ids.select(iSensorAddr);
  ids.write(0xBE);

  // Le MOT reçu du capteur fait 8 octets, on les charge donc un par un dans le tableau data[]
  for ( i = 0; i < 9; i++) {
    data[i] = ids.read();
  }

  return computeTemperature(data[1], data[0]);
}

float computeTemperature(byte msb, byte lsb) {
  int buffer = msb << 8 | lsb;
  Serial.print("buffer = ");
  Serial.println(buffer);

  int negativeSign = (0xF8 & msb) != 0;
  Serial.print("negativeSign = ");
  Serial.println(negativeSign);

  if (negativeSign) {
    float temp = ((buffer ^ 0xffff) + 1) * -0.0625;
    Serial.print("temperature = ");
    Serial.println(temp);
    return temp;
  }
  else {
    float temp = (buffer & 0xFFF) * 0.0625; // 12 bits resolution
    Serial.print("temperature = ");
    Serial.println(temp);
    return temp;
  }
}

/* --------------- LCD --------------- */

/**
 * LCD - setup
 */
void lcd_setup() {
  lcd.begin(20,4); // initialize the lcd for 20 chars 4 lines
  lcd.backlight(); // finish with backlight on  
  lcd.clear();
  
  // NOTE: Cursor Position: CHAR, LINE) start at 0  
  lcd.setCursor(0,0); //Start at character 0 on line 0
  lcd.print("T courante :");

  lcd.setCursor(0,1); //Start at character 0 on line 1
  lcd.print("T limite   :");
  lcd.setCursor(13,1); //Start at character 13 on line 1
  lcd.print(temperatureLimit);

  lcd.setCursor(0,2); //Start at character 0 on line 2
  lcd.print("Min:      Max:");

  lcd.setCursor(0,3); //Start at character 0 on line 3
  lcd.print("Fan OFF");  

  lcd.setCursor(8,3); //Start at character 4 on line 3
  lcd.print("Mode: Sensor");
}

/**
 * LCD - display temperature
 */
void lcd_display_temp(float temp, float tempMin, float tempMax) {
  lcd.setCursor(4,2); //Start at character 4 on line 2
  lcd.print(tempMin);

  lcd.setCursor(14,2); //Start at character 14 on line 2
  lcd.print(tempMax);

  lcd.setCursor(13,0); //Start at character 13 on line 0
  lcd.print(temp);
}

void lcd_display_fanMode(int fanModeOrder) {
  lcd.setCursor(4,3); //Start at character 4 on line 3

  if(fanModeOrder == ON)
    lcd.print("ON ");
  else
    lcd.print("OFF");

  lcd.setCursor(14,3); //Start at character 4 on line 3
  if(fanMode == byTempSensor)
    lcd.print("Sensor");
  else
    lcd.print("User  ");
}

/* --------------- Fan Drive --------------- */
void driveFan(int fanModeOrder) {
  if(fanModeOrder == ON) {
    digitalWrite(FANRELAY1_PIN, HIGH);
//    digitalWrite(FANRELAY2_PIN, HIGH);
  } else {
    digitalWrite(FANRELAY1_PIN, LOW);
//    digitalWrite(FANRELAY2_PIN, LOW);
  }
}

/* --------------- Miscellanious I2C Bus - Unsed --------------- */

/**
 * Scan the I2C bus to find connected devices addresses.
 */
/*
void scanI2C() {
  byte count = 0;

  Serial.println("------------------------------");  
  Serial.println("I2C Scanning...");
  
  Wire.begin();
  for (byte i = 8; i < 120; i++)
  {
    Wire.beginTransmission (i);
    if (Wire.endTransmission () == 0)
      {
      Serial.print ("Found address: ");
      Serial.print (i, DEC);
      Serial.print (" (0x");
      Serial.print (i, HEX);
      Serial.println (")");
      count++;
      delay (1);  // maybe unneeded?
      } // end of good response
  } // end of for loop
  
  Serial.println ("...Done.");
  Serial.print ("Found ");
  Serial.print (count, DEC);
  Serial.println (" device(s).");

  Serial.println("------------------------------");
}
*/