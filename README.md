# f3xwind
Wind measurement tool for F3x soaring competitions according to FAI rules
https://www.fai.org/sites/default/files/sc4_vol_f3_soaring_25.pdf


CIAM General rules

C.17.2 Interruption
a) The contest should be interrupted or the start delayed by the contest director in the following
circumstances and in other exceptional circumstances decided by the contest director:
i) Unless specified otherwise in the rules for a category or a particular class, the wind is
continuously stronger than 12 m/s measured at two (2) metres above the ground at the starting
line (flight line), for at least one minute

F3 Soaring

F3B.1.11. Weather Conditions / Interruptions
a) The maximum wind speed for F3B contests is twelve (12) m/sec. The contest has to be interrupted
or the start delayed by the contest director if the wind speed exceeds twelve (12) m/sec measured three (3)
times for at least twenty (20) seconds in a time interval of five (5) minutes two (2) metres above the ground
at the start and landing area.

F3F.1.17. Weather Conditions and interruptions
A round in progress must be interrupted if:-
The wind speed is below three (3) m/sec or more than twenty-five (25) m/sec for at least twenty (20)
a) seconds.
b) The direction of the wind deviates more than 45º from a line perpendicular to the main direction of
the speed course for at least twenty (20) seconds.
The wind speed and wind direction are measured with the equipment of the organiser at a representative
position and height chosen from the experience of the organiser.

F3G.1.12. Weather Conditions/Interruptions
a) The maximum wind speed for F3G contests is twelve (12) m/sec. The contest has to be interrupted
or the start delayed by the contest director if the wind speed exceeds twelve (12) m/sec measured three (3)
times for at least twenty (20) seconds in a time interval of five (5) minutes two (2) metres above the ground
at the start and landing area.

F3J.12. WEATHER CONDITIONS AND INTERRUPTIONS
The maximum wind speed for F3J contests is twelve (12) m/sec two (2) m above the ground at the centre of
the launch corridor. The start of the contest must be delayed or the contest has to be interrupted by the
contest director if the wind speed exceeds twelve (12) m/sec measured three (3) times for at least twenty
(20) sec in a time interval of five (5) minutes at the start and landing area.

F3K.5. WEATHER CONDITIONS / INTERRUPTIONS
The maximum wind speed for F3K contests is eight (8) m/sec. The start of the contest must be delayed or
the contest has to be interrupted by the contest director if the wind speed exceeds eight (8) m/sec measured
three (3) times for at least twenty (20) sec in a time interval of five (5) minutes at two (2) metres above the
ground at the start and landing field. In the case of rain, the contest director can interrupt the contest. When
the rain stops, the contest starts again with the group that was flying, which receives a re-flight.

F3L.6 Interruptions
a) The Contest Director has the right to interrupt the competition and relocate the starting line
when the wind direction deviates too much or becomes a tailwind.
b) The competition shall be interrupted by the Contest Director if the wind is continuously
stronger than eight (8) m/s measured at two (2) metres above the ground at the starting line (flight
line), for at least one minute.


Helicopter 

INTERRUPTION OF A COMPETITION
If the wind component perpendicular to the flight line exceeds 8ms/s for a minimum of 20 seconds
during a flight, the competition must be interrupted. The flight will be repeated and the competition
continued as soon as the wind subsides below the criterion. If the wind does not subside before the
round is completed, the entire round will be dropped. The determination will be made by the organiser
with concurrence of the FAI Jury.

Free flight

F1.5 INTERRUPTION OF A CONTEST
F1.5.1 Wind speed
The interruption of contests is defined in CIAM General Rules C.17.2. For Free Flight contests the
contest should be interrupted when the wind measured at 2 metres above the ground at the starting
line is stronger than 9 m/s for at least 20 seconds.

Control line

4.0.1 INTERRUPTION OF THE CONTEST
Wind stronger than 9 m/s for at least 30 seconds (instead of 12 m/s for at least one minute in
CIAM General Rules).

## Specification

* Battery powered. Last one complete competition day of 10 hours
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


## Setup LittleFS Filesystem

For html File, follow this tutorial https://randomnerdtutorials.com/esp32-vs-code-platformio-littlefs/


## Capacity Measurement

Screen brightness, 200

Start, 100% battery level, 01.04.2025, 08:05
70% battery level, 10:05
55% battery level, 12:15
42% battery level, 14:02
28% battery level, 16:28
23% battery level, 17:01
1% battery level, 18:28
0% battery level, 18:41 --> shutdown, last log entry

--> 10:30h Laufzeit

Start 100%, Brightness 128, 01.04.2025, 23:30
25%, 02.04.2025, 12:30 
17%, 02.04.2025, 13:30
0%, 02.04.2025, 14:00 Uhr


38%, 14:35 beim ausschalten --> Schauen, ob der Akku trotzdem lädt
56% 15:30 beim einschalten... sieht so aus, als ob geladen würde aber nicht mit dem strom der eingestellt ist (wobei.. vielleicht 500mA das wären 4h zum volladen und 25% pro Stunde)
91% 17:08, 500mA 

03.04.2025

Start um 08:55 Uhr mit 100% und 255 display brightness
11:30 --> 79% 1,5h 20%... 
14:00 --> 59%   5h 40%...
14:30 --> 54%
15:10 --> 46%
16:00 --> 40%
18:15 --> 19%



## Introduction
## BOM
* M5Tough
* Windspeed Measurement Sensor
* Battery
* Pogo-Pins
* USB-Port
* Thread Brass inserts
* Schrauben
* Alloy Tube 30mm diameter
* 3D Print parts
## System Setup
* Power on
* Setup WiFi connection via local access point
* 
## Functions

## Mechanical Setup
### CAD, STL Files 

