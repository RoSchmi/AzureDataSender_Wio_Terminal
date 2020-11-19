### AzureDataSender_Wio_Terminal

Transfer telemetry sensor data to Azure Storage Tables. App running on the Wio Terminal developed on PlatformIO using Arduino Code, Microsoft Azure C SDK and espressif arduino-esp32 HTTPClient. This is still work in progress, creating tables and inserting Entities is already working.

This App works with the new Seeed eRPC WiFi library. Actually the App works only with http, not https.
A working App with secure transmission using the old atWiFi Firmware can be seen here:
https://github.com/RoSchmi/PlatformIO/tree/master/Proj/Wio_Terminal_Azure_DataSender_02


The stored data can be visualized in table form with the App 'AzureTabStorClient' or as line charts with the App 'Charts4Azure'.
https://azuretabstorclient.wordpress.com/
https://azureiotcharts.home.blog/

Before you start, install the firmware v2.0.1 for the Wio Terminal, firmware v2.0.2 didn't work for me
https://wiki.seeedstudio.com/Wio-Terminal-Wi-Fi/

When you have cloned this repository in your working directory proceed as follows:
Copy the file include/config_secret_template.h to a file named include/config_secret.h
Open config_secret.h and enter your WiFi Credentials and the Credentials of your Azure Storage Account.
Open the file include/config.h and enter some settings (esp. the send interval, your timezone and daylightsavings offset) according to your needs.

#### Actually (19.11.2020) the libraries from 'Seeed_Arduino_mbedtls', were the dev branch has to be used must be copied manually in the .pio/libdeps folder.
#### The automatic installation via PlatformIO did actually not work for me, when a dev branch has to be used.

Now you should be able to compile and deploy the App to the Wio Terminal. The App should automatically create a table in your Azure Storage Account and start to create new rows.

This App is in process of development, not yet stable


