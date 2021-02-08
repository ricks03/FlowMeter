# FlowMeter
Arduino / Misterhouse project for monitoring a FlowMeter

This is a flowmeter ecosystem. 
The physical flowmeter is an Arduino ATMega 2560, with an 5100 Ethernet shield w/SSD daighter board
and a custom daughterboard for two flow meters (with a Fritzing sketch). 

The flow meter has a LCD screen, as well as a webserver, NTP, data backup and logging. 

Misterhouse code polls the webserver. 
