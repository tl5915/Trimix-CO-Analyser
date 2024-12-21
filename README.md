# Trimix-CO-Analyser
Trimix and CO analyser for scuba diving

Material:
- Electronic Connection Box 158x90x60mm (housing)
- 15 mm ID, 18 mm OD PVC pipe (for sensor chamber, O2 mount to the end, He and CO on two sides, gas out on the other end)
- M16 x 1.0 tap (for O2 sensor)
- 16 mm ID silicone plug (drill hole for gas out)
- BCD nipple to 1.4 NPT male convertor (gas in)
- 3/8 UNF tap (tap on the NPT side of the above convertor to mount a o-ring-removed LP blanking plug to adjust flow rate to 2 L/min)
- 1/4 NPT female to the smallest hose barb you can find, use barb convertor if necessary (ideally 2 mm ID silicone tubing connect to sensor chamber)
- Oxygen sensor, any type, just get the right connection, they all read by differential voltage between + and -
- Wisen MD62 Helium sensor (mount to sensor chamber with hole cut out and seal with hot glue)
- Wisen ZE07 Carbon Monoxide sensor (mount to sensor chamber with hole cut out and seal with hot glue) (use analog output, pin definition in technical manual, connect to any Xiao analog input pin)
- Latching button (power switch)
- Momentary button (one for screen page switch, one for all in one calibration)
- Seeed Xiao SAMD21
- 0.96 inch I2C SSD1306 OLED
- ADS1115 ADC (analog inputs connects to two O2 pins and two He pins)
- 9V to 5V DC-DC convertor
- 3.7V 1300 mAh LiPo battery
- 1N5819 Schottky diode (drops the 3.3v output of Xiao to 3.0v that MD62 requires)
- 2k ohm resistors and 500 ohn variable resisotr (for MD62 Wheatstone bridge, schematics in technical manual)
- 3.7V 1100 mAh LiPo battery
