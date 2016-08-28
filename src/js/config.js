module.exports = [
  {
    "type": "heading",
    "defaultValue": "App Configuration"
  },
  {
    "type": "text",
    "defaultValue": "Setting some of the colour values in the watch face"
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Bars"
      },
      {
        "type": "color",
        "messageKey": "BatteryColour",
        "defaultValue": "0x000000",
        "label": "Battery Bar"
      },
      {
        "type": "color",
        "messageKey": "HourColour",
        "defaultValue": "0x000000",
        "label": "Hour Bar"
      },
      {
        "type":"color",
        "messageKey": "MinuteColour",
        "defaultValue": "0x000000",
        "label": "Minute Bar"
      },
      {
        "type": "color",
        "messageKey": "StepPieColour",
        "defaultValue": "0x000000",
        "label": "Step Counter Pie Chart"
      },
      {
        "type": "color",
        "messageKey": "BluetoothColour",
        "defaultValue": "0x000000",
        "label": "Bluetooth Disconnect"
      },
      {
        "type": "color",
        "messageKey": "BackgroundColour",
        "defaultValue": "0x000000",
        "label": "Background"
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Text Colours"
      },
      {
        "type": "color",
        "messageKey": "TimeColour",
        "defaultValue": "0x000000",
        "label": "Time"
      },
      {
        "type": "color",
        "messageKey": "DateColour",
        "defaultValue": "0x000000",
        "label": "Date"
      },
      {
        "type": "color",
        "messageKey": "StepColour",
        "defaultValue": "0x000000",
        "label": "Step Counter"
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save Settings"
  }
];