/* 
   Home_Assistant.ino

   An example of using HTTP POST requests to send environment data
   from the Metriful MS430 to an installation of Home Assistant on
   your local network.

   This example is designed for the following WiFi enabled hosts:
   * Arduino UNO with Ethernet Shield

   Data are sent at regular intervals over your network to Home
   Assistant and can be viewed on the dashboard or used to control
   home automation tasks. More setup information is provided in the
   Readme.

   Copyright 2020-2023 Metriful Ltd.
   Licensed under the MIT License - for further details see LICENSE.txt

   For code examples, datasheet and user guide, visit
   https://github.com/metriful/sensor
*/

#include <Metriful_sensor.h>
#include <SPI.h>
#include <Ethernet.h>

//////////////////////////////////////////////////////////
// USER-EDITABLE SETTINGS

// How often to read and report the data (every 3, 100 or 300 seconds)
uint8_t cycle_period = CYCLE_PERIOD_100_S;

// Home Assistant settings

// You must have already installed Home Assistant on a computer on your 
// network. Go to www.home-assistant.io for help on this.

// Choose a unique name for this MS430 sensor board so you can identify it.
// Variables in HA will have names like: SENSOR_NAME.temperature, etc.
#define SENSOR_NAME "storage"

// Change this to the IP address of the computer running Home Assistant. 
// You can find this from the admin interface of your router.
#define HOME_ASSISTANT_IP "192.168.1.98"

// Security access token: the Readme explains how to get this
#define LONG_LIVED_ACCESS_TOKEN "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiI4MDM2MjVjMDY5YjE0NjA3OGM5YmZmZjM0MzhjNjkyNiIsImlhdCI6MTcwNzU2NTU1NSwiZXhwIjoyMDIyOTI1NTU1fQ.SfyEE1HA6sKoNz7OGFAq_iS3ac9559gvLsojJ99qrTo"

// END OF USER-EDITABLE SETTINGS
//////////////////////////////////////////////////////////

const byte BUFFER_SIZE = 32;
char icon_buffer[BUFFER_SIZE];
char name_buffer[BUFFER_SIZE];

// Buffers for assembling http POST requests
char postBuffer[256] = {0};
char fieldBuffer[128] = {0};

// Structs for data
AirData_t airData = {0};
AirQualityData_t airQualityData = {0};
LightData_t lightData = {0}; 
ParticleData_t particleData = {0};
SoundData_t soundData = {0};

// Define the display attributes of data to send to Home Assistant. 
// The chosen name, unit and icon will appear in on the overview 
// dashboard in Home Assistant. The icons can be chosen at
// https://pictogrammers.com/library/mdi/
// The attribute fields are: {name, unit, icon, decimal places}
// HA_Attributes_t pressure = {"Air pressure", "Pa", "weather-partly-rainy", 0};
// HA_Attributes_t humidity = {"Humidity", "%", "cloud-percent", 1};
// HA_Attributes_t illuminance = {"Illuminance", "lux", "white-balance-sunny", 2};
// HA_Attributes_t white_light = {"White light level", "", "circle-outline", 0};
// HA_Attributes_t soundLevel = {"Sound pressure level", "dBA", "microphone", 1};
// HA_Attributes_t peakAmplitude = {"Peak sound amplitude", "mPa", "waveform", 2};
// HA_Attributes_t AQI = {"Air Quality Index", "", "flower-tulip-outline", 1};
// HA_Attributes_t AQ_assessment = {"Air quality", "", "flower-tulip-outline", 0};
// HA_Attributes_t AQ_accuracy = {"Air quality accuracy", "", "magnify", 0};
// HA_Attributes_t gas_resistance = {"Gas sensor resistance", OHM_SYMBOL, "scent", 0};
// #if (PARTICLE_SENSOR == PARTICLE_SENSOR_PPD42)
//   HA_Attributes_t particulates = {"Particle concentration", "ppL", "chart-bubble", 0};
// #else
//   HA_Attributes_t particulates = {"Particle concentration", SDS011_UNIT_SYMBOL,
//                                   "chart-bubble", 2};
// #endif
// #ifdef USE_FAHRENHEIT
//   HA_Attributes_t temperature = {"Temperature", FAHRENHEIT_SYMBOL, "thermometer", 1};
// #else
//   HA_Attributes_t temperature = {"Temperature", CELSIUS_SYMBOL, "thermometer", 1};
// #endif
// HA_Attributes_t soundBands[SOUND_FREQ_BANDS] = {{"SPL at 125 Hz", "dB", "sine-wave", 1},
//                                                 {"SPL at 250 Hz", "dB", "sine-wave", 1},
//                                                 {"SPL at 500 Hz", "dB", "sine-wave", 1},
//                                                 {"SPL at 1000 Hz", "dB", "sine-wave", 1},
//                                                 {"SPL at 2000 Hz", "dB", "sine-wave", 1},
//                                                 {"SPL at 4000 Hz", "dB", "sine-wave", 1}};

const char HA_ATTRIBUTE_AIR_QUALITY_NAME[] PROGMEM = "Air quality";
const char HA_ATTRIBUTE_AIR_QUALITY_ACCURACY_NAME[] PROGMEM = "Air quality accuracy";
const char HA_ATTRIBUTE_AIR_QUALITY_INDEX_NAME[] PROGMEM = "Air Quality Index";
const char HA_ATTRIBUTE_AIR_PRESSURE_NAME[] PROGMEM = "Air pressure";
const char HA_ATTRIBUTE_GAS_SENSOR_RESISTANCE_NAME[] PROGMEM = "Gas sensor resistance";
const char HA_ATTRIBUTE_HUMIDITY_NAME[] PROGMEM = "Humidity";
const char HA_ATTRIBUTE_ILLUMINANCE_NAME[] PROGMEM = "Illuminance";
const char HA_ATTRIBUTE_PARTICLE_CONCENTRATION_NAME[] PROGMEM = "Particle concentration";
const char HA_ATTRIBUTE_PEAK_SOUND_AMPLITUDE_NAME[] PROGMEM = "Peak sound amplitude";
const char HA_ATTRIBUTE_SOUND_PRESSURE_LEVEL_NAME[] PROGMEM = "Sound pressure level";
const char HA_ATTRIBUTE_TEMPERATURE_NAME[] PROGMEM = "Temperature";
const char HA_ATTRIBUTE_WHITE_LIGHT_LEVEL_NAME[] PROGMEM = "White light level";

PGM_P const HA_ATTRIBUTE_NAMES[] PROGMEM = {
  HA_ATTRIBUTE_AIR_QUALITY_NAME,       // idx = 0
  HA_ATTRIBUTE_AIR_QUALITY_ACCURACY_NAME,     // idx = 1
  HA_ATTRIBUTE_AIR_QUALITY_INDEX_NAME,     // idx = 2
  HA_ATTRIBUTE_AIR_PRESSURE_NAME,        // idx = 3
  HA_ATTRIBUTE_GAS_SENSOR_RESISTANCE_NAME,     // idx = 4
  HA_ATTRIBUTE_HUMIDITY_NAME,     // idx = 5
  HA_ATTRIBUTE_ILLUMINANCE_NAME,       // idx = 6
  HA_ATTRIBUTE_PARTICLE_CONCENTRATION_NAME,     // idx = 7
  HA_ATTRIBUTE_PEAK_SOUND_AMPLITUDE_NAME,        // idx = 8
  HA_ATTRIBUTE_SOUND_PRESSURE_LEVEL_NAME, // idx = 9
  HA_ATTRIBUTE_TEMPERATURE_NAME, // idx = 10
  HA_ATTRIBUTE_WHITE_LIGHT_LEVEL_NAME,     // idx = 11
};

const char HA_ATTRIBUTE_AIR_QUALITY_ICON[] PROGMEM = "flower-tulip-outline";
const char HA_ATTRIBUTE_AIR_QUALITY_ACCURACY_ICON[] PROGMEM = "magnify";
const char HA_ATTRIBUTE_AIR_QUALITY_INDEX_ICON[] PROGMEM = "flower-tulip-outline";
const char HA_ATTRIBUTE_AIR_PRESSURE_ICON[] PROGMEM = "weather-partly-rainy";
const char HA_ATTRIBUTE_GAS_SENSOR_RESISTANCE_ICON[] PROGMEM = "scent";
const char HA_ATTRIBUTE_HUMIDITY_ICON[] PROGMEM = "cloud-percent";
const char HA_ATTRIBUTE_ILLUMINANCE_ICON[] PROGMEM = "white-balance-sunny";
const char HA_ATTRIBUTE_PARTICLE_CONCENTRATION_ICON[] PROGMEM = "chart-bubble";
const char HA_ATTRIBUTE_PEAK_SOUND_AMPLITUDE_ICON[] PROGMEM = "waveform";
const char HA_ATTRIBUTE_SOUND_PRESSURE_LEVEL_ICON[] PROGMEM = "microphone";
const char HA_ATTRIBUTE_TEMPERATURE_ICON[] PROGMEM = "thermometer";
const char HA_ATTRIBUTE_WHITE_LIGHT_LEVEL_ICON[] PROGMEM = "circle-outline";

PGM_P const HA_ATTRIBUTE_ICONS[] PROGMEM = {
  HA_ATTRIBUTE_AIR_QUALITY_ICON,       // idx = 0
  HA_ATTRIBUTE_AIR_QUALITY_ACCURACY_ICON,     // idx = 1
  HA_ATTRIBUTE_AIR_QUALITY_INDEX_ICON,     // idx = 2
  HA_ATTRIBUTE_AIR_PRESSURE_ICON,        // idx = 3
  HA_ATTRIBUTE_GAS_SENSOR_RESISTANCE_ICON,     // idx = 4
  HA_ATTRIBUTE_HUMIDITY_ICON,     // idx = 5
  HA_ATTRIBUTE_ILLUMINANCE_ICON,       // idx = 6
  HA_ATTRIBUTE_PARTICLE_CONCENTRATION_ICON,     // idx = 7
  HA_ATTRIBUTE_PEAK_SOUND_AMPLITUDE_ICON,        // idx = 8
  HA_ATTRIBUTE_SOUND_PRESSURE_LEVEL_ICON, // idx = 9
  HA_ATTRIBUTE_TEMPERATURE_ICON, // idx = 10
  HA_ATTRIBUTE_WHITE_LIGHT_LEVEL_ICON,     // idx = 11
};

typedef enum {
  HA_ATTRIBUTE_AIR_QUALITY_IDX = 0,
  HA_ATTRIBUTE_AIR_QUALITY_ACCURACY_IDX = 1,
  HA_ATTRIBUTE_AIR_QUALITY_INDEX_IDX = 2,
  HA_ATTRIBUTE_AIR_PRESSURE_IDX = 3,
  HA_ATTRIBUTE_GAS_SENSOR_RESISTANCE_IDX = 4,
  HA_ATTRIBUTE_HUMIDITY_IDX = 5,
  HA_ATTRIBUTE_ILLUMINANCE_IDX = 6,
  HA_ATTRIBUTE_PARTICLE_CONCENTRATION_IDX = 7,
  HA_ATTRIBUTE_PEAK_SOUND_AMPLITUDE_IDX = 8,
  HA_ATTRIBUTE_SOUND_PRESSURE_LEVEL_IDX = 9,
  HA_ATTRIBUTE_TEMPERATURE_IDX = 10,
  HA_ATTRIBUTE_WHITE_LIGHT_LEVEL_IDX = 11,
} ha_attribute_names;

// HA_Attributes_t ha_attributes = {"", "", "", 0};

// the media access control (ethernet hardware) address for the shield:
byte mac[] = { 0x90, 0xA2, 0xDA, 0x0F, 0xFC, 0xA0 };  

EthernetClient client;

void setup()
{
  // Initialize the host's pins, set up the serial port and reset:
  SensorHardwareSetup(I2C_ADDRESS); 

  Ethernet.begin(mac);
  
  // Apply settings to the MS430 and enter cycle mode
  uint8_t particleSensorCode = PARTICLE_SENSOR;
  TransmitI2C(I2C_ADDRESS, PARTICLE_SENSOR_SELECT_REG, &particleSensorCode, 1);
  TransmitI2C(I2C_ADDRESS, CYCLE_TIME_PERIOD_REG, &cycle_period, 1);
  ready_assertion_event = false;
  TransmitI2C(I2C_ADDRESS, CYCLE_MODE_CMD, 0, 0);
}


void loop()
{
  // Wait for the next new data release, indicated by a falling edge on READY
  while (!ready_assertion_event)
  {
    yield();
  }
  ready_assertion_event = false;

  // Read data from the MS430 into the data structs. 
  ReceiveI2C(I2C_ADDRESS, AIR_DATA_READ, (uint8_t *) &airData, AIR_DATA_BYTES);
  ReceiveI2C(I2C_ADDRESS, AIR_QUALITY_DATA_READ, (uint8_t *) &airQualityData,
             AIR_QUALITY_DATA_BYTES);
  ReceiveI2C(I2C_ADDRESS, LIGHT_DATA_READ, (uint8_t *) &lightData, LIGHT_DATA_BYTES);
  ReceiveI2C(I2C_ADDRESS, SOUND_DATA_READ, (uint8_t *) &soundData, SOUND_DATA_BYTES);
  ReceiveI2C(I2C_ADDRESS, PARTICLE_DATA_READ, (uint8_t *) &particleData, PARTICLE_DATA_BYTES);
 
  uint8_t T_intPart = 0;
  uint8_t T_fractionalPart = 0;
  bool isPositive = true;
  getTemperature(&airData, &T_intPart, &T_fractionalPart, &isPositive);
  
  // Send data to Home Assistant
  HA_Attributes_t ha_attributes = {"", "", "", 0};

  // for (int i=0; i < attributes; ++i) {}
    name_buffer[0] = '\0';
    strcpy_P(name_buffer,
            (char *)pgm_read_word(&(HA_ATTRIBUTE_NAMES[HA_ATTRIBUTE_AIR_QUALITY_IDX])));
    icon_buffer[0] = '\0';
    strcpy_P(icon_buffer,
            (char *)pgm_read_word(&(HA_ATTRIBUTE_ICONS[HA_ATTRIBUTE_AIR_QUALITY_IDX])));
    ha_attributes = {name_buffer, "", icon_buffer, 0};
    sendTextData(&ha_attributes, interpret_AQI_value(airQualityData.AQI_int));

    name_buffer[0] = '\0';
    strcpy_P(name_buffer,
            (char *)pgm_read_word(&(HA_ATTRIBUTE_NAMES[HA_ATTRIBUTE_AIR_QUALITY_ACCURACY_IDX])));
    icon_buffer[0] = '\0';
    strcpy_P(icon_buffer,
            (char *)pgm_read_word(&(HA_ATTRIBUTE_ICONS[HA_ATTRIBUTE_AIR_QUALITY_ACCURACY_IDX])));
    ha_attributes = {name_buffer, "", icon_buffer, 0};
    sendTextData(&ha_attributes, interpret_AQI_accuracy_brief(airQualityData.AQI_accuracy));

    name_buffer[0] = '\0';
    strcpy_P(name_buffer,
            (char *)pgm_read_word(&(HA_ATTRIBUTE_NAMES[HA_ATTRIBUTE_AIR_QUALITY_INDEX_IDX])));
    icon_buffer[0] = '\0';
    strcpy_P(icon_buffer,
            (char *)pgm_read_word(&(HA_ATTRIBUTE_ICONS[HA_ATTRIBUTE_AIR_QUALITY_INDEX_IDX])));
    ha_attributes = {name_buffer, "", icon_buffer, 1};
    sendNumericData(&ha_attributes, (uint32_t) airQualityData.AQI_int, airQualityData.AQI_fr_1dp, true);

    name_buffer[0] = '\0';
    strcpy_P(name_buffer,
            (char *)pgm_read_word(&(HA_ATTRIBUTE_NAMES[HA_ATTRIBUTE_AIR_PRESSURE_IDX])));
    icon_buffer[0] = '\0';
    strcpy_P(icon_buffer,
            (char *)pgm_read_word(&(HA_ATTRIBUTE_ICONS[HA_ATTRIBUTE_AIR_PRESSURE_IDX])));
    ha_attributes = {name_buffer, "Pa", icon_buffer, 0};
    sendNumericData(&ha_attributes, (uint32_t) airData.P_Pa, 0, true);

    name_buffer[0] = '\0';
    strcpy_P(name_buffer,
            (char *)pgm_read_word(&(HA_ATTRIBUTE_NAMES[HA_ATTRIBUTE_GAS_SENSOR_RESISTANCE_IDX])));
    icon_buffer[0] = '\0';
    strcpy_P(icon_buffer,
            (char *)pgm_read_word(&(HA_ATTRIBUTE_ICONS[HA_ATTRIBUTE_GAS_SENSOR_RESISTANCE_IDX])));
    ha_attributes = {name_buffer, OHM_SYMBOL, icon_buffer, 0};
    sendNumericData(&ha_attributes, airData.G_ohm, 0, true);

    name_buffer[0] = '\0';
    strcpy_P(name_buffer,
            (char *)pgm_read_word(&(HA_ATTRIBUTE_NAMES[HA_ATTRIBUTE_HUMIDITY_IDX])));
    icon_buffer[0] = '\0';
    strcpy_P(icon_buffer,
            (char *)pgm_read_word(&(HA_ATTRIBUTE_ICONS[HA_ATTRIBUTE_HUMIDITY_IDX])));
    ha_attributes = {name_buffer, "%", icon_buffer, 1};
    sendNumericData(&ha_attributes, (uint32_t) airData.H_pc_int, airData.H_pc_fr_1dp, true);

    name_buffer[0] = '\0';
    strcpy_P(name_buffer,
            (char *)pgm_read_word(&(HA_ATTRIBUTE_NAMES[HA_ATTRIBUTE_ILLUMINANCE_IDX])));
    icon_buffer[0] = '\0';
    strcpy_P(icon_buffer,
            (char *)pgm_read_word(&(HA_ATTRIBUTE_ICONS[HA_ATTRIBUTE_ILLUMINANCE_IDX])));
    ha_attributes = {name_buffer, "lux", icon_buffer, 2};
    sendNumericData(&ha_attributes, (uint32_t) lightData.illum_lux_int,
                    lightData.illum_lux_fr_2dp, true);

    if (PARTICLE_SENSOR != PARTICLE_SENSOR_OFF)
    {
      name_buffer[0] = '\0';
      strcpy_P(name_buffer,
              (char *)pgm_read_word(&(HA_ATTRIBUTE_NAMES[HA_ATTRIBUTE_PARTICLE_CONCENTRATION_IDX])));
      icon_buffer[0] = '\0';
      strcpy_P(icon_buffer,
              (char *)pgm_read_word(&(HA_ATTRIBUTE_ICONS[HA_ATTRIBUTE_PARTICLE_CONCENTRATION_IDX])));
      ha_attributes = {name_buffer, SDS011_UNIT_SYMBOL,
                                    icon_buffer, 2};
      sendNumericData(&ha_attributes, (uint32_t) particleData.concentration_int, 
                      particleData.concentration_fr_2dp, true);
    }

    name_buffer[0] = '\0';
    strcpy_P(name_buffer,
            (char *)pgm_read_word(&(HA_ATTRIBUTE_NAMES[HA_ATTRIBUTE_PEAK_SOUND_AMPLITUDE_IDX])));
    icon_buffer[0] = '\0';
    strcpy_P(icon_buffer,
            (char *)pgm_read_word(&(HA_ATTRIBUTE_ICONS[HA_ATTRIBUTE_PEAK_SOUND_AMPLITUDE_IDX])));
    ha_attributes = {name_buffer, "mPa", icon_buffer, 2};
    sendNumericData(&ha_attributes, (uint32_t) soundData.peak_amp_mPa_int, 
                    soundData.peak_amp_mPa_fr_2dp, true);

    name_buffer[0] = '\0';
    strcpy_P(name_buffer,
            (char *)pgm_read_word(&(HA_ATTRIBUTE_NAMES[HA_ATTRIBUTE_SOUND_PRESSURE_LEVEL_IDX])));
    icon_buffer[0] = '\0';
    strcpy_P(icon_buffer,
            (char *)pgm_read_word(&(HA_ATTRIBUTE_ICONS[HA_ATTRIBUTE_SOUND_PRESSURE_LEVEL_IDX])));
    ha_attributes = {name_buffer, "dBA", icon_buffer, 1};
    sendNumericData(&ha_attributes, (uint32_t) soundData.SPL_dBA_int, soundData.SPL_dBA_fr_1dp, true);

    name_buffer[0] = '\0';
    strcpy_P(name_buffer,
            (char *)pgm_read_word(&(HA_ATTRIBUTE_NAMES[HA_ATTRIBUTE_TEMPERATURE_IDX])));
    icon_buffer[0] = '\0';
    strcpy_P(icon_buffer,
            (char *)pgm_read_word(&(HA_ATTRIBUTE_ICONS[HA_ATTRIBUTE_TEMPERATURE_IDX])));
    ha_attributes = {name_buffer, CELSIUS_SYMBOL, icon_buffer, 1};
    sendNumericData(&ha_attributes, (uint32_t) T_intPart, T_fractionalPart, isPositive);

    name_buffer[0] = '\0';
    strcpy_P(name_buffer,
            (char *)pgm_read_word(&(HA_ATTRIBUTE_NAMES[HA_ATTRIBUTE_WHITE_LIGHT_LEVEL_IDX])));
    icon_buffer[0] = '\0';
    strcpy_P(icon_buffer,
            (char *)pgm_read_word(&(HA_ATTRIBUTE_ICONS[HA_ATTRIBUTE_WHITE_LIGHT_LEVEL_IDX])));
    ha_attributes = {name_buffer, "", icon_buffer, 0};
    sendNumericData(&ha_attributes, (uint32_t) lightData.white, 0, true);


    // ha_attributes[SOUND_FREQ_BANDS] = {{"SPL at 125 Hz", "dB", "sine-wave", 1},
    //                                     {"SPL at 250 Hz", "dB", "sine-wave", 1},
    //                                     {"SPL at 500 Hz", "dB", "sine-wave", 1},
    //                                     {"SPL at 1000 Hz", "dB", "sine-wave", 1},
    //                                     {"SPL at 2000 Hz", "dB", "sine-wave", 1},
    //                                     {"SPL at 4000 Hz", "dB", "sine-wave", 1}};
    // for (uint8_t i = 0; i < SOUND_FREQ_BANDS; i++)
    // {
    //   sendNumericData(&ha_attributes[i], (uint32_t) soundData.SPL_bands_dB_int[i],
    //                   soundData.SPL_bands_dB_fr_1dp[i], true);
    // }
  // }
}

// Send numeric data with specified sign, integer and fractional parts
void sendNumericData(const HA_Attributes_t * attributes, uint32_t valueInteger, 
                     uint8_t valueDecimal, bool isPositive)
{
  char valueText[20] = {0};
  const char * sign = isPositive ? "" : "-";
  switch (attributes->decimalPlaces)
  {
    case 0:
    default:
      snprintf(valueText, sizeof valueText, "%s%" PRIu32, sign, valueInteger);
      break;
    case 1:
      snprintf(valueText, sizeof valueText, "%s%" PRIu32 ".%u", sign,
               valueInteger, valueDecimal);
      break;
    case 2:
      snprintf(valueText, sizeof valueText, "%s%" PRIu32 ".%02u", sign,
               valueInteger, valueDecimal);
      break;
  }
  http_POST_Home_Assistant(attributes, valueText);
}

// Send a text string: must have quotation marks added.
void sendTextData(const HA_Attributes_t * attributes, const char * valueText)
{
  char quotedText[20] = {0};
  snprintf(quotedText, sizeof quotedText, "\"%s\"", valueText);
  http_POST_Home_Assistant(attributes, quotedText);
}

// Send the data to Home Assistant as an HTTP POST request.
void http_POST_Home_Assistant(const HA_Attributes_t * attributes, const char * valueText)
{
  client.stop();
  if (client.connect(HOME_ASSISTANT_IP, 8123))
  {
    // Form the entity_id from the variable name but replace spaces with underscores
    snprintf(fieldBuffer, sizeof fieldBuffer, SENSOR_NAME ".%s", attributes->name);
    for (uint8_t i = 0; i < strlen(fieldBuffer); i++)
    {
      if (fieldBuffer[i] == ' ')
      {
        fieldBuffer[i] = '_';
      }
    }
    snprintf(postBuffer, sizeof postBuffer, "POST /api/states/%s HTTP/1.1", fieldBuffer);
    client.println(postBuffer);
    client.println(F("Host: " HOME_ASSISTANT_IP ":8123"));
    client.println(F("Content-Type: application/json"));
    client.println(F("Authorization: Bearer " LONG_LIVED_ACCESS_TOKEN));

    // Assemble the JSON content string:
    snprintf(postBuffer, sizeof postBuffer, "{\"state\":%s,\"attributes\":"
             "{\"unit_of_measurement\":\"%s\",\"friendly_name\":\"%s\",\"icon\":\"mdi:%s\"}}",
             valueText, attributes->unit, attributes->name, attributes->icon);

    snprintf(fieldBuffer, sizeof fieldBuffer, "Content-Length: %u", strlen(postBuffer));
    client.println(fieldBuffer);
    client.println();
    client.print(postBuffer);
  }
  else
  {
    Serial.println(F("Client connection failed."));
  }
}
