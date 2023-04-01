/*
 * DAD_Utils.c
 *
 *  Created on: Mar 20, 2023
 *      Author: Max
 */


#include <DAD_Utils/DAD_LUTs.h>

// Initialize frequency Lookup Table
    // Lookup table connects char to str output
    // avoids having to go through sprintf every time
void DAD_Utils_initFreqLUT(DAD_utilsStruct* utilsStruct){
    int i;
//    // Generate end characters
//    char endChars[3];
//    for(i = 0; i < 3; i++){
//        endChars[i] = 255;
//    }
    //endChars[4] = '/0';

    for(i = 0; i < FREQ_LUT_SIZE; i++){
        // microSD LUT
        sprintf(utilsStruct->microSDFreqLUT[i], "%d,", i);

        // HMI LUT
        int j;
        for(j = 0; j < HMI_LUT_WORD_SIZE + 1; j++){
            utilsStruct->HMIFreqLUT[i][j] = '\0';
        }
        sprintf(utilsStruct->HMIFreqLUT[i], "add %d,0,%d", HMI_FFT_ID, i);
//        strcat(utilsStruct->HMIFreqLUT[i], endChars);
    }
}

// Using given uint8_t, return corresponding string to write to microSD
char* DAD_Utils_getMicroSDStr(uint8_t num, DAD_utilsStruct* utilsStruct){
    return utilsStruct->microSDFreqLUT[num];
}

// Using given uint8_t, return corresponding string to write to HMI
char* DAD_Utils_getHMIStr(uint8_t num, DAD_utilsStruct* utilsStruct){
    return utilsStruct->HMIFreqLUT[num];
}
