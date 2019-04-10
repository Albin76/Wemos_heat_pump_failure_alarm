# Wemos_heat_pump_failure_alarm
Sensor that stores in Thingspeak. Thingspeak reacts to low temperature (Heat pump failure) or no updates (No wifi or powerloss). IFTTT sends notifications to the reacts in Thingspeak. 

Full description on: https://www.instructables.com/id/Wemos-Heat-Pump-Failure-Alarm

Note (2019-04-10):

ArduinoJson.h (https://github.com/bblanchon/ArduinoJson) max version 5.13.5 (6.x not working! due to DynamicJsonBuffer is removed in version 6.) 

Core for Esp8266 max version 2.4.0 otherwise gets "failed to mount FS". See issue #863 in WiFiManager.h (https://github.com/tzapu/WiFiManager/issues/863)
