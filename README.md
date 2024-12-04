# f3xwind
Wind measurement tool for F3x soaring competitions according to FAI rules

## Specification

* Battery powered. Last one complete competition day of 10h
* Recharchable by USB-C
* Additional Backup Power by USB-C
* Magnetic connection between sensor and display
* 3D Print holder with simple aluminium tube
* Logging of Windspeed on SD-Card. One file per day,
* Sampling Rate of windspeed 1s
* Time synchronisation via ntp server and local backup with RTC 
* Webserver to display the current windspeed and download of all available SD-Card files
* Display with current windspeed value and 5 minute historical data
* Display which shows the min/max/avrg value during the last 5 minutes (rolling)
* Display which shows the regions where the 8 m/s threashold is reached for over 20s
* Accoustic alarm if the the threshold is reached for over 20s
* Accoustic alarm if the condition is active more than 3 times in the last 5 minutes
* Github project with 3D Files, BOM and Code
* Overall costs of Hardware about 100€ (40€ Microcontroller & Display, 40€ Windsensor, 20€ Accessories)
* Water resistant (ToDo)

## Ideas and ToDos

* QR Code generation to get Link to webserver
* Replace transmission of complete array and send only current value to webpage
* Display current values and statisic values
* Log files with timestamp
* Prepare files for download 
* Use WPS for Wifi connection
* Different displays (number and statistic only, pure plot chart, mixed chart)
* Settings page (time, threshold, sound volume, unit, wps button)