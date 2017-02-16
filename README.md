# mbed-os-evn-sensor
#Ubirch Environmental Sensor

The Environmental sensor on the Ubirch#1 board measures the temperature, pressure and humidity asynchronously in a thread once every 10 seconds.

The board first signs these sensor values and send them to the Ubirch-Backend using MQTT message protocol.
The message is sent to the backend in a predefined interval of time, these intervals can also be configured by sending a configuration message to the device from the server. 
If the sensor temperature is more than the threshold then the message is sent more often.

#Building
- clone mbed-os and switch to branch `target-ubirch` to get the specific ubirch #1 changes
- link mbed-os into this directory at the top-level
- `ln -sf <MBED-OS_DIR_PATH>/mbed-os .`
- add target `mbed target UBIRCH1`
- add toolchain `mbed toolchain GCC_ARM`
#Importing libraries
we use three mbed libraries in this project BME280, MQTT and WolfSSL. To import these libraries go to project directory and run
`mbed add http://mbed.org/users/MACRUM/code/BME280/`
`mbed add http://mbed.org/teams/mqtt/code/MQTT/`
`mbed add http://mbed.org/users/wolfSSL/code/wolfSSL/`

#Add Git Submodule
We use mbed-os-quectelM66-driver to enable the modem and establish IP connection. 
mbed-os-quectelM66-driver directory is already there but empty. Run the below commands
- `git submodule` to initialize your local configuration file
- `git submodule update`  to fetch all the data from that project

#Compiling
- to compile the program using mbed build tool run `mbed compile`
- to clean and rebuild the directory again run `mbed compile -c`
# Flashing
You can find the flash script in `./bin` directory
- run `./bin/flash.sh` to flash using NXP blhost tool
- alternatively, if you have SEGGER tools installed, run `./bin/flash.sh -j`
#Debugging
- To compile Debug Release
`mbed compile --profile mbed-os/tools/profiles/debug.json`
- use `-c` to recompile everything
- Create a `gdb.init` file and add this
`target extended-remote localhost:2331`
`monitor halt`

- In terminal start the GDB Server
`JLinkGDBServer -if SWD -device MK82FN256xxx15`
- run `cgdb -d arm-none-eabi-gdb -x /home/user/gdb.init ./BUILD/UBIRCH1/GCC_ARM/mbed-os-porting.elf`
