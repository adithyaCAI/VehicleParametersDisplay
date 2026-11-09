[page 1]
1. Objective 
Monitor interior cabin climate and vehicle ride dynamics using STM32-NUCLEO-G070RB, 
and print the data over the OLED display for the monitoring purpose. 
2. Hardware 
 
● MCU: STM32-NUCLEO-G070RB dev board 
● Climate sensor: DHT22 (temperature, humidity) 
● IMU: MPU6050 (3-axis accelerometer and gyroscope) 
● OLED 1.3” (SH1106 driver) Display  
● Power: 5V via micro-USB (from 12V automotive adapter) 
 
Pin Mapping 
 
Signal   STM32-NUCLEO-G070RB Pin 
DHT22 VCC   3.3V 
DHT22 GND   GND 
DHT22 OUT   PB4 
MPU6050 VCC  3.3V 
MPU6050 GND  GND 
MPU6050 SCL  PB8 (shared with OLED) 
  MPU6050 SDA  PB9 (shared with OLED) 
  OLED VDD   3.3V 
  OLED GND   GND 
  OLED SCL   PB8 (shared with MPU6050) 
  OLED SDA   PB9 (shared with MPU6050)   
  Inbuilt User LED (LD4)  PA5 
3. Functional Requirements 
 Read temperature (°C) and humidity (%) from the DHT22 every 2 seconds. 
 Read X, Y, Z acceleration from the MPU6050 (±4 g range, 100 Hz) and convert to 
G-force. At power-up, keep the board flat and still for 1 s so the readings are zeroed 
(X = 0 g, Y = 0 g, Z = 1 g). 
 Map axes: X = lateral (cornering), Y = longitudinal (accel/braking), Z = vertical 
(bumps). 
 Print the sensor data on the OLED with the heading "Vehicle Parameters Display". 
 Keep all alert thresholds configurable as macros in config.h (values below). 
 "Suddenly" means the axis value changes by at least the threshold within 100 ms. 
The original 15 g was replaced because 15 g is a crash-level force and would never 
trigger. 
 Display "Unseen SpeedBreaker" if Z changes suddenly by 0.30 g or more. 
 Display "Wide Turn" if X changes suddenly by 0.25 g or more. 
 Display "Speeding" if Y changes suddenly by +0.25 g or more (detected from 
acceleration, so it indicates harsh acceleration, not actual speed). 
 Display "Sudden Braking" if Y changes suddenly by −0.25 g or more. 
 If any of the above 4 events occurs, show the event message immediately on the 
OLED while keeping the Temperature and Humidity values visible, and blink the 
built-in LED (LD4) every second (500 ms on, 500 ms off). The message and 
blinking stay for 2 s after the last event; if events overlap, the priority is Sudden 
Braking, Unseen SpeedBreaker, Wide Turn, Speeding. 
 If temperature or humidity goes above its limit, show "!" next to the value. 
 

[page 2]
Parameter   Demo threshold   How to trigger in the demo

Temperature high   32 °C   Hold the sensor between warm fingers

Humidity high   75 %   Breathe gently on the DHT22

Z(Unseen SpeedBreaker) 0.30 g   Quick tap or lift-and-drop

X (Wide Turn)   0.25 g   Quick flick left or right

+Y (Speeding)   +0.25 g   Quick push forward

−Y (Sudden Braking)   −0.25 g   Quick pull backward

Sudden window   100 ms

Event display time   2 s

For in-vehicle use, raise the G thresholds to about 0.4 to 0.5 g and the temperature and
humidity limits to 45 °C and 80 %.

4. OLED Display format

Vehicle Parameters
Display

Temp: “value from DHT22” °C
Humidity: “: “value from DHT22” %
X:<x>
Y:<y>
Z:<z> g

5. Expected Use
Installed in a vehicle, streaming cabin comfort and ride data to a dashboard for driver
behavior review or ride comfort tracking, with immediate alerts on extreme cabin or
dynamics events.