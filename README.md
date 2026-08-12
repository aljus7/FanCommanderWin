![Project logo](https://github.com/aljus7/FanCommander/blob/main/Logo/logo.png)

<br>Example config file:
``` json
{
  "settings": {
    "refreshInterval": 500,
    "oneSensorReadPerCycle": true
  },

  "tempSensors": [
    {
      "sensor": "CPU 1",
      "sensorName": "Core Average",
      "deviceIndex": 0,
      "graph": [
        {
          "temp": 48,
          "pwm": 88
        },
        {
          "temp": 50,
          "pwm": 92
        },
        {
          "temp": 56,
          "pwm": 110
        },
        {
          "temp": 60,
          "pwm": 130
        },
        {
          "temp": 70,
          "pwm": 255
        }
      ]
    },
    {
      "sensor": "GPU 1",
      "sensorName": "GPU Core",
      "deviceIndex": 0,
      "graph": [
        {
          "temp": 48,
          "pwm": 90
        },
        {
          "temp": 50,
          "pwm": 92
        },
        {
          "temp": 56,
          "pwm": 125
        },
        {
          "temp": 60,
          "pwm": 180
        },
        {
          "temp": 70,
          "pwm": 255
        }
      ]
    }
  ],

  "fans": [
    {
      "fanControlIndex": 3,
      "fanRpmIndex": 3,
      "sensors": [ "GPU 1", "CPU 1" ],
      "sensorFunction": "max",
      "averageSampleSize": 20,
      "minPwm": 60,
      "startPwm": 70,
      "maxPwm": 255,
      "overrideMax": true,
      "spinUpDelay": [
        {
          "fromPwm": 90,
          "toPwm": 150,
          "duration": 4000
        },
        {
          "fromPwm": 140,
          "toPwm": 200,
          "duration": 3000
        }
      ],
      "spinDownDelay": [
        {
          "fromPwm": 120,
          "toPwm": 170,
          "duration": 4000
        },
        {
          "fromPwm": 170,
          "toPwm": 255,
          "duration": 10000
        }
      ],
      "proportionalFactor": 0.02,
      "hysteresis": 0.08
    },
    {
      "fanControlIndex": 0,
      "fanRpmIndex": 0,
      "sensors": [ "GPU 1", "CPU 1" ],
      "sensorFunction": "max",
      "averageSampleSize": 10,
      "minPwm": 60,
      "startPwm": 70,
      "maxPwm": 255,
      "overrideMax": true,
      "spinUpDelay": [
        {
          "fromPwm": 90,
          "toPwm": 150,
          "duration": 4000
        },
        {
          "fromPwm": 140,
          "toPwm": 200,
          "duration": 3000
        }
      ],
      "spinDownDelay": [
        {
          "initiation": 200,
          "duration": 4000
        },
        {
          "initiation": 150,
          "duration": 1000
        }
      ],
      "proportionalFactor": 0.02,
      "hysteresis": 0.08
    }
  ]
}
```
