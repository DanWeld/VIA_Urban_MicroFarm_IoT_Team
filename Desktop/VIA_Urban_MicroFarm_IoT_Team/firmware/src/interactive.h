/*****************************************************************************
 * interactive.h
 *  interface for the Interactive application file for the IoT hardware
 *  drivers demo.
 *  The demo presents a menu where you can select which driver to
 *  test.
 *  Connect a terminal (e.g. yat) to the board on uart0 and follow the
 *  instructions for each driver.
 
 *****************************************************************************/
#pragma once

void interactive_demo(void);
void pir_callback(void);
