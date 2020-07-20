/*
 *  Hello world example of a TLS client: fetch an HTTPS page
 *
 *  Copyright (C) 2006-2018, Arm Limited, All Rights Reserved
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  This file is part of Mbed TLS (https://tls.mbed.org)
 */

/**
 * \file main.cpp
 *
 * \brief An example TLS Client application
 *
 * This application sends an HTTPS request to os.mbed.com and searches
 * for a string in the result.
 *
 * This example is implemented as a logic class (HelloHttpsClient) wrapping a
 * TCP socket. The logic class handles all events, leaving the main loop to just
 * check if the process  has finished.
 */

#include "mbed.h"

#include "mbedtls/platform.h"
#if defined(MBEDTLS_USE_PSA_CRYPTO)
#include "psa/crypto.h"
#endif /* MBEDTLS_USE_PSA_CRYPTO */

#include "mbed_trace.h"
#include "trace_helper.h"
#include "ONBOARD_TELIT_ME910.h"
#include "HelloHttpsClient.h"

EventQueue queue(32 * EVENTS_EVENT_SIZE);
Thread t(osPriorityNormal, 64 * 1024 /*32K stack size*/);
DigitalOut led(LED1);
DigitalOut cellular_on_off_key(PIN_NAME_CELL_ON_OFF);
Timeout system_reset_timeout;

/* Domain/IP address of the server to contact */
const char SERVER_NAME[] = "os.mbed.com";
const char SERVER_ADDR[] = "os.mbed.com";

/* Port used to connect to the server */
const int SERVER_PORT = 443;

//splash screen to clearly identify software resets over the UART
void planet_splash(void)
{
    printf("\033[0;34m");

printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@&             #@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\r\n");
printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@/*                     .*/&@@@@@@@@@@@%***#@@@@@@@@@@\r\n");
printf("@@@@@@@@@@@@@@@@@@@@@@&%.          ...,*((*,..          #&@@@@@@#     /@@@@@@@@@\r\n");
printf("@@@@@@@@@@@@@@@@@@@@/        *%%@@@@@@@@@@@@@@@@@%#.       *@@@@@(   *&@@@@@@@@@\r\n");
printf("@@@@@@@@@@@@@@@@@@*      ,%@@@@@@@@@@@@@@@@@@@@@@@@@@&*      .@@@@@@@@@@@@@@@@@@\r\n");
printf("@@@@@@@@@@@@@@@@(      %@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@&      *@@@@@@@@@@@@@@@@\r\n");
printf("@@@@@@@@@@@@@@@/     (@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@/     .&@@@@@@@@@@@@@@\r\n");
printf("@@@@@@@@@@@@@@/      (#((*,.         ,@@@@@@@@@@@@@@@@@@@@@@     ,&@@@@@@@@@@@@@\r\n");
printf("@@@@@@@@@@@@@(                        *@@@@@@@@@@@@@@@@@@@@@@*    ,@@@@@@@@@@@@@\r\n");
printf("@@@@@@@@@@@@%.     ......,,,,,*/(#%&@@@@@@@@@@@@@@@@@@@@@@@@@#     ,&&&%%%%@@@@@\r\n");
printf("@@@@@@@@@@@@(    /&@@@@@@@@@@@@@&%%#/*,,,,,,,,,#@@@@*.                      #@@@\r\n");
printf("@#(//*,..                                       #@@@.                .,,,***%@@@\r\n");
                                     printf(".,*(#%&&@@@@@@@@@@@@@@@@@#    *@@@@@@@@@@@@\r\n");
printf("@((((((((*        *&@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@&.    (@%/*#@@@@@@@\r\n");
printf("@@@@@@@%          ,&@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@/    *@@(  *&@@@@@@\r\n");
printf("@@@@@@@@/          .%@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@,    ,&@@@@@@@@@@@@@\r\n");
printf("@@@@@@@@@@@@@@@/     (@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@%     ,@@@@@@@@@@@@@@@\r\n");
printf("@@@@@@@@@@@@@@@@#.     .&@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@&.     ,@@@@@@@@@@@@@@@@\r\n");
printf("@@@@(,  .%@@@@@@@@(.      (@@@@@@@@@@@@@@@@@@@@@@@@@@#/      /@@@@@@@@@@@@@@@@@@\r\n");
printf("@@@%      (@@@@@@@@@,         &@@@@@@@@@@@@@@@@@@@,         @@@@@@@@@@@@@@@@@@@@\r\n");
printf("@@@@/   .#@@@@@@@@@@@@@#           *////////*.          (@@@@@@@@@@@@@@@@@@@@@@@\r\n");
printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@&..                     .#@@@@@@@@@@@@@@@@@@@@@@@@@@@\r\n");
printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@%%##(////(##%%&@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\r\n");
printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\r\n");
    printf("\033[0m");
}

void agora_bsp_cell_enable(bool enabled)
{
    DigitalOut cell_power_enable(PIN_NAME_CELL_POWER_ENABLE);
    cell_power_enable = enabled;
}

void system_reset_callback (void)
{
    system_reset();
}

void run_example_in_event_queue(void)
{
    system_reset_timeout.attach(&system_reset_callback, 300);
    static int exit_code  =0;
    static int status = 0;
    HelloHttpsClient *client;
    if((exit_code = mbedtls_platform_setup(NULL)) != 0) {
        printf("Platform initialization failed with error %d\r\n", exit_code);
        printf("terminating program\r\n");
        while(1){}

        //return MBEDTLS_EXIT_FAILURE;
    }
    client = new (std::nothrow) HelloHttpsClient(SERVER_NAME, SERVER_ADDR, SERVER_PORT);

    if (client == NULL) {
        mbedtls_printf("Failed to allocate HelloHttpsClient object\n"
                       "\nterminating \n");
        mbedtls_platform_teardown(NULL);
        while(1){}
        //return exit_code;
    }
    /* Run the client */
    status = client->run();
    if (status != 0) {
        mbedtls_printf("\nFAIL run(), retrying...\n");
    } 
    else {
        exit_code = MBEDTLS_EXIT_SUCCESS;
        mbedtls_printf("\nDONE\n");
    }
    //cancel watchdog timeout
    system_reset_timeout.detach();

    //manually call destructor to cleanup
    //client->~HelloHttpsClient();
    delete client;
    client = NULL;
    // Send graceful power off signal to the cell module
    cellular_on_off_key = 0;
    ThisThread::sleep_for(3500ms); // 3.5 seconds
    cellular_on_off_key = 1;
    // Turn off power to the cell
    agora_bsp_cell_enable(false);

    //call this function from the queue, again
    queue.call(run_example_in_event_queue);
}

/**
 * The main function driving the HTTPS client.
 */
int main()
{
    //splash screen
    planet_splash();

    // Start the event queue in thread context
    t.start(callback(&queue, &EventQueue::dispatch_forever));
    int exit_code = MBEDTLS_EXIT_FAILURE;

    setup_trace();
    mbed_trace_config_set(TRACE_ACTIVE_LEVEL_DEBUG);

    /*
     * The default 9600 bps is too slow to print full TLS debug info and could
     * cause the other party to time out.
     */
    mbedtls_printf("Starting mbed-os-example-tls/tls-client\n");

#if defined(MBED_MAJOR_VERSION)
    mbedtls_printf("Using Mbed OS %d.%d.%d\n",
                   MBED_MAJOR_VERSION, MBED_MINOR_VERSION, MBED_PATCH_VERSION);
#else
    printf("Using Mbed OS from master.\n");
#endif /* MBEDTLS_MAJOR_VERSION */

    queue.call(run_example_in_event_queue);

    //blink led to indicate that main thread is still operating while TLS function operates in thread
    while(1)
    {
        ThisThread::sleep_for(500ms);
        led = !led;
    }
}
