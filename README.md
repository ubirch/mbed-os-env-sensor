# mbed-os-evn-sensor
#Ubirch Environmental Sensor

The Environmental sensor on the Ubirch#1 board measures the temperature, pressure and humidity asynchronously in a thread once every 10 seconds.

The board first signs these sensor values and send them to the Ubirch-Backend using MQTT message protocol.
Public-key cryptography is implemented to exchange the messages between the board and the backend securely.

The message is sent to the backend in a predefined interval of time, these intervals can also be configured by sending a configuration message to the device from the server. 
If the sensor temperature is more than the threshold limit then the message is sent more often.

#Getting Started
- Run ``mbed import https://github.com/ubirch/mbed-os-env-sensor.git`` to import the  Ubirch Environmental Sensor project.
This also downloads all the libraries used in this example.
- Run `mbed target UBIRCH1` to add target
- Run `mbed toolchain GCC_ARM`to add toolchain 
- Enable `WOLFSSL_BASE64_ENCODE`functionality and low memory footprint in WolfSSL by adding 

  `#define WOLFSSL_BASE64_ENCODE` and `#define CURVED25519_SMALL` to wolfSSL/user_settings.h
- The board gets the credentials like **Cell APN, username, password, MQTT username, password, host URL and port number** from `config.h` file, which is ignored by git.
 
   Copy/ rename the [config.h.template](https://github.com/ubirch/mbed-os-env-sensor/blob/master/config.h.template) file as `config.h` and add the credentials 

#Building
- to compile the program using mbed build tool run `mbed compile`
- to clean and rebuild the directory again run `mbed compile -c`

# Flashing
You can find the flash script in `bin` directory
- run `./bin/flash.sh` to flash using NXP blhost tool
- alternatively, if you have SEGGER tools installed, run `./bin/flash.sh -j`

# Debugging
- To compile Debug Release
`mbed compile --profile mbed-os/tools/profiles/debug.json`
- use `-c` to recompile everything
- Create a `gdb.init` file and add this
`target extended-remote localhost:2331`
`monitor halt`

- In terminal start the GDB Server
`JLinkGDBServer -if SWD -device MK82FN256xxx15`
- run `cgdb -d arm-none-eabi-gdb -x /home/user/gdb.init ./BUILD/UBIRCH1/GCC_ARM/mbed-os-porting.elf`

# Env-sensor as HTTP client
 - this uses **mbed-http** library
 - example project can be found in *example* directory