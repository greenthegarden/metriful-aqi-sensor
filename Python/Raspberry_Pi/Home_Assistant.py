"""Example of sending data from the Metriful MS430 to Home Assistant.

This example is designed to run with Python 3 on a Raspberry Pi and
requires an installation of Home Assistant (www.home-assistant.io)
on your home network.

Data are sent at regular intervals over your local network, can be
viewed on the Home Assistant dashboard, and can be used to control
home automation tasks. More setup information is provided in the
Readme and User Guide.
"""

#  Copyright 2020-2023 Metriful Ltd.
#  Licensed under the MIT License - for further details see LICENSE.txt

#  For code examples, datasheet and user guide, visit
#  https://github.com/metriful/sensor

import requests
import time
import sensor_package.sensor_functions as sensor
import sensor_package.sensor_constants as const

#########################################################
# USER-EDITABLE SETTINGS

# How often to read and report the data (every 3, 100 or 300 seconds)
cycle_period = const.CYCLE_PERIOD_100_S

# Home Assistant settings

# You must have already installed Home Assistant on a computer on your
# network. Go to www.home-assistant.io for help on this.

# Choose a unique name for this MS430 sensor board so you can identify it.
# Variables in HA will have names like: SENSOR_NAME.temperature, etc.
SENSOR_NAME = "homelab"

# Specify the IP address of the computer running Home Assistant.
# You can find this from the admin interface of your router.
HOME_ASSISTANT_IP = "192.168.1.98"

# Security access token: the Readme and User Guide explain how to get this
LONG_LIVED_ACCESS_TOKEN = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJkM2RjNTIwMzdhYTA0MjExOGMzMjJmODdlZDc4MGE2NiIsImlhdCI6MTcwNzYyMjA2OSwiZXhwIjoyMDIyOTgyMDY5fQ.ESp-eTfdTaobyLCvOTgwfQoGvJuRsh7RiolnPCGxh6U"

# END OF USER-EDITABLE SETTINGS
#########################################################

# Set up the GPIO and I2C communications bus
(GPIO, I2C_bus) = sensor.SensorHardwareSetup()

# Apply the settings to the MS430
I2C_bus.write_i2c_block_data(sensor.i2c_7bit_address,
                             const.PARTICLE_SENSOR_SELECT_REG,
                             [sensor.PARTICLE_SENSOR])
I2C_bus.write_i2c_block_data(sensor.i2c_7bit_address,
                             const.CYCLE_TIME_PERIOD_REG, [cycle_period])

#########################################################

print("Reporting data to Home Assistant. Press ctrl-c to exit.")

# Enter cycle mode
I2C_bus.write_byte(sensor.i2c_7bit_address, const.CYCLE_MODE_CMD)

while (True):

    # Wait for the next new data release, indicated by a falling edge on READY
    while (not GPIO.event_detected(sensor.READY_pin)):
        time.sleep(0.05)

    # Now read all data from the MS430
    air_data = sensor.get_air_data(I2C_bus)
    air_quality_data = sensor.get_air_quality_data(I2C_bus)
    light_data = sensor.get_light_data(I2C_bus)
    sound_data = sensor.get_sound_data(I2C_bus)
    particle_data = sensor.get_particle_data(I2C_bus, sensor.PARTICLE_SENSOR)

    # Specify information needed by Home Assistant.
    # Icons are chosen from https://cdn.materialdesignicons.com/5.3.45/
    # (remove the "mdi-" part from the icon name).
    pressure = dict(name='Pressure', data=air_data['P_Pa'], unit='Pa',
                    icon='weather-cloudy', decimals=0)
    humidity = dict(name='Humidity', data=air_data['H_pc'], unit='%',
                    icon='water-percent', decimals=1)
    temperature = dict(name='Temperature', data=air_data['T'],
                       unit=air_data['T_unit'], icon='thermometer', decimals=1)
    illuminance = dict(name='Illuminance', data=light_data['illum_lux'],
                       unit='lx', icon='white-balance-sunny', decimals=2)
    sound_level = dict(name='Sound level', data=sound_data['SPL_dBA'],
                       unit='dBA', icon='microphone', decimals=1)
    sound_peak = dict(name='Sound peak', data=sound_data['peak_amp_mPa'],
                      unit='mPa', icon='waveform', decimals=2)
    AQI = dict(name='Air Quality Index', data=air_quality_data['AQI'],
               unit=' ', icon='thought-bubble-outline', decimals=1)
    AQI_interpret = dict(name='Air quality assessment',
                         data=sensor.interpret_AQI_value(
                             air_quality_data['AQI']),
                         unit='', icon='flower-tulip', decimals=0)
    particle = dict(name='Particle concentration',
                    data=particle_data['concentration'],
                    unit=particle_data['conc_unit'], icon='chart-bubble',
                    decimals=2)

    # Send data to Home Assistant using HTTP POST requests
    variables = [pressure, humidity, temperature, illuminance,
                 sound_level, sound_peak, AQI, AQI_interpret]
    if sensor.PARTICLE_SENSOR != const.PARTICLE_SENSOR_OFF:
        variables.append(particle)
    try:
        for v in variables:
            url = ("http://" + HOME_ASSISTANT_IP + ":8123/api/states/" +
                   SENSOR_NAME + "." + v['name'].replace(' ', '_'))
            head = {"Content-type": "application/json",
                    "Authorization": "Bearer " + LONG_LIVED_ACCESS_TOKEN}
            try:
                valueStr = "{:.{dps}f}".format(v['data'], dps=v['decimals'])
            except Exception:
                valueStr = v['data']
            payload = {"state": valueStr, "attributes": {
                "unit_of_measurement": v['unit'], "friendly_name": v['name'],
                "icon": "mdi:" + v['icon']}}
            requests.post(url, json=payload, headers=head, timeout=2)
    except Exception as e:
        # An error has occurred, likely due to a lost network connection,
        # and the post has failed.
        # The program will retry with the next data release and will succeed
        # if the network reconnects.
        print("HTTP POST failed with the following error:")
        print(repr(e))
        print("The program will continue and retry on the next data output.")
