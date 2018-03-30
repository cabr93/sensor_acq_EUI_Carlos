/* mbed Example Program
 * Copyright (c) 2006-2014 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <stdint.h>
#include <cstring>
#include <sstream>
#include "mbed.h"

#include "dot_util.h"
#include "RadioEvent.h"

#include "MTSLog.h"
#include "MTSText.h"


using namespace std;

// Initialize a pins to perform analog input and digital output fucntions
AnalogIn temp(PB_1);  // This corresponds to A0 Connector on the Grove Shield
AnalogIn light(PB_0);  // This corresponds to A1 Connector on the Grove Shield
AnalogIn sound(PC_1);  // This corresponds to A2 Connector on the Grove Shield
DigitalOut ledsystem(LED1);

float templevel;
float lightlevel;
float soundlevel;
float sleeping_time=5.0;

static std::string network_name = "MTCDT-ceaiot";
static std::string network_passphrase = "ceaiot2017";
static uint8_t network_id[] = { 0x02, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01 };
static uint8_t network_key[] = { 0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C };
static uint8_t frequency_sub_band = 1;
static bool public_network = true;
static uint8_t ack = 1;

mDot* dot = NULL;


Serial pc(USBTX, USBRX);

float readTempSensor(){
    float tempsensor;
    int B=39751;
    float resistance;
    int sensorValue;
    sensorValue = temp.read_u16();
    resistance = (float)(65534-sensorValue)*10000/sensorValue; //get the resistance of the sensor;
    tempsensor=1/(log(resistance/10000)/B+1/298.15)-273.15;//convert to temperature via datasheet
    //printf("Temp Sensor reading: %d - %2.2f\r\n", sensorValue, tempsensor);
    return tempsensor;
}

float readLightSensor(){
    float sensorValue;
    float lightsensor; 
    sensorValue = light.read();
    lightsensor = (float)(1023-sensorValue)*10/sensorValue;
    //printf("Light Sensor reading: %2.2f - %2.2f\r\n", sensorValue, lightsensor);
    return lightsensor;
}

float readSoundSensor(){
    float sensorValue;
    float soundsensor; 
    sensorValue = sound.read();
    soundsensor = (float) sensorValue;
    //printf("Sound Sensor reading: %2.2f - %2.2f\r\n", sensorValue, soundsensor);
    return soundsensor;
}


int* fillarr(int intArray[], float data)
{
    std::string floattostr;
    std::ostringstream buff;
    buff<<data;
    floattostr=buff.str();
    const char* float_str = floattostr.c_str();
    string text = (string) float_str;
    //printf("Este es el numero: %s\r\n",float_str); 
    //string charArray = "2.3";
    for(int idx = 0; idx < text.length(); idx++)
    {
      char x =text.at(idx);
      intArray[idx] = int(x);
    }
    return intArray;
}

///////////////////////////////////////////////////////////////////////////////////////

int main(void)
{
    
    int32_t ret;
    int32_t next_tx;
    int32_t wait_time = 10;
    
    uint8_t recv = 0;
    uint8_t recv_mismatch = 0;
    uint8_t send_failure = 0;
    //uint8_t iterations = 50;

    
    ledsystem=false;
    
    RadioEvent events;

    pc.baud(115200);
    
    
    mts::MTSLog::setLogLevel(mts::MTSLog::TRACE_LEVEL);
    dot = mDot::getInstance();
    logInfo("mbed-os library version: %d", MBED_LIBRARY_VERSION);

    // start from a well-known state
    logInfo("defaulting Dot configuration");
    dot->resetConfig();
    dot->resetNetworkSession();

    // make sure library logging is turned on
    dot->setLogLevel(mts::MTSLog::INFO_LEVEL);

    // attach the custom events handler
    dot->setEvents(&events);

    // update configuration if necessary
    if (dot->getJoinMode() != mDot::OTA) {
        logInfo("changing network join mode to OTA");
        if (dot->setJoinMode(mDot::OTA) != mDot::MDOT_OK) {
            logError("failed to set network join mode to OTA");
        }
    }
        
    // in OTA and AUTO_OTA join modes, the credentials can be passed to the library as a name and passphrase or an ID and KEY
    // only one method or the other should be used!
    // network ID = crc64(network name)
    // network KEY = cmac(network passphrase)
    //update_ota_config_name_phrase(network_name, network_passphrase, frequency_sub_band, public_network, ack);
    update_ota_config_id_key(network_id, network_key, frequency_sub_band, public_network, ack);

    // configure the Dot for class C operation
    // the Dot must also be configured on the gateway for class C
    // use the lora-query application to do this on a Conduit: http://www.multitech.net/developer/software/lora/lora-network-server/
    // to provision your Dot for class C operation with a 3rd party gateway, see the gateway or network provider documentation
    logInfo("changing network mode to class C");
    if (dot->setClass("C") != mDot::MDOT_OK) {
        logError("failed to set network mode to class C");
    }
    
    // save changes to configuration
    logInfo("saving configuration");
    if (!dot->saveConfig()) {
        logError("failed to save configuration");
    }

    // display configuration
    display_config();

    int count_loop=0;

    while (1) {
        
        std::vector<uint8_t> recv_data;

        count_loop=count_loop+1;
        logInfo("COMM. Loop number: %d", count_loop); //printf("\r\n\r\n");
        
        std::vector<uint8_t> tx_data;
        
        // join network if not joined
        if (!dot->getNetworkJoinStatus()) {
            join_network();
        }
        

        // sensor read

        templevel = readTempSensor();
        lightlevel = readLightSensor();
        soundlevel = readSoundSensor();
        
        //logInfo("temp: %f [0x%04X]", templevel, templevel);
        //logInfo("lux: %f [0x%04X]", lightlevel, lightlevel);
        //logInfo("sound: %d [0x%04X]", soundlevel, soundlevel);
        
        // sensor word formatiom    
        int artemp[10];
        int arlux[10];
        int arsound[10];
   
        int *intArrLux= fillarr(arlux,lightlevel);
        tx_data.push_back(intArrLux[0]);
        tx_data.push_back(intArrLux[1]);
        tx_data.push_back(intArrLux[2]);
        tx_data.push_back(intArrLux[3]);
           
        int *intArrTemp= fillarr(artemp,templevel);
        tx_data.push_back(intArrTemp[0]);
        tx_data.push_back(intArrTemp[1]);
        tx_data.push_back(intArrTemp[3]);
        
        int *intArrSound= fillarr(arsound,soundlevel);
        tx_data.push_back(intArrSound[2]); //punto .
        tx_data.push_back(intArrSound[3]);
        
        //send_data(tx_data);
        
        if ((ret = dot->send(tx_data)) != mDot::MDOT_OK) {
            logError("failed to send: [%d][%s]", ret, mDot::getReturnCodeString(ret).c_str());
            send_failure++;
        } else {
            //logInfo("send data: %s", Text::bin2hexString(tx_data).c_str());
            //printf("\r\n send data: %s\r\n",tx_data);
            //logWarning("send data: %d", tx_data);
            wait(1); // wait to receive data
            
            if ((ret = dot->recv(recv_data)) != mDot::MDOT_OK) {
                logError("failed to recv: [%d][%s]", ret, mDot::getReturnCodeString(ret).c_str());
            } else {
                //logInfo("recv data: %s", Text::bin2hexString(recv_data).c_str());
                //printf("\r\n recv data: %s \r\n",recv_data); 
                //logWarning("received data: %s", recv_data);
                if (recv_data == tx_data) {
                    recv++;
                } else {
                    recv_mismatch++;
                }
            }
                
        }

        //if(ret){
            ledsystem = !ledsystem;
        //}
        
        recv_data.clear();
        //logWarning("verify clear the recv data: %s", recv_data);
        
        next_tx = dot->getNextTxMs() + 1;
        //logInfo("waiting %ld ms to transmit again", next_tx);
        wait_ms(2);
        //logInfo("waiting another %d seconds", wait_time);
        wait(1);


    }  // end while
    
    //logInfo("Version: %s", dot->getId().c_str());
    //printf("\r\n recv data: %s \r\n", dot->getId().c_str());
    //logInfo("Recv: %d/%d", recv, iterations);
    //logInfo("Recv Mismatch: %d/%d", recv_mismatch, iterations);
    //logInfo("Send Failure: %d/%d", send_failure, iterations);
    //logInfo("Dropped: %d/%d", iterations - (recv + recv_mismatch + send_failure), iterations);
    return 0;
}