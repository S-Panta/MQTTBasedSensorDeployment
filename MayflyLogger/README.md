## Deployment Guide
Connect two sensors dts12 and cs451 on same grove connector. Their SDI-12 address should be different. In the sketch,dts12 has sdi 12 address of 1 and cs451 has sensor address of 2.
Logging period is 15 minutes.
The datastream id for each measured observation should be taken from HydroServer.


## Setup BEE in Mayfly logger
- find mac address of your esp32 and register it to the USU network.
 