#include <DAD_Calc.h>

// Initializes struct's values
void DAD_Calc_InitStruct(DAD_Calc_Struct* calcStruct){
    calcStruct->numSamplesCollected = 0;
    calcStruct->type = START;
}

// Moving average
    // Takes newest reading, updates sensor's moving average
float DAD_Calc_MovingAvg(uint8_t* packet, DAD_Calc_Struct* calcStruct){
    // Check packet type
    packetType type = (packetType)(packet[0] & PACKET_TYPE_MASK);
    float data = conditionPacket(packet);

    if(type != calcStruct->type || calcStruct->numSamplesCollected == 0){
        calcStruct->numSamplesCollected = 1;
        calcStruct->type = type;
        calcStruct->list[0]  = data;
        return data;
    }

    // Add sample
    calcStruct->list[calcStruct->numSamplesCollected % MOVING_AVERAGE_N]  = data;
    calcStruct->numSamplesCollected++;

    // Calc avg
    double sum = 0;
    int i;

    for (i = 0; i < calcStruct->numSamplesCollected; i++) {
        sum += calcStruct->list[i];
    }

    return sum / calcStruct->numSamplesCollected;   // return sum/N
}

// Average Intensity
    // Takes reading, updates sensor's average intensity
float DAD_Calc_AvgIntensity(uint8_t dataSet[FFT_SIZE-2], packetType type){
    // Calculate average intensity
    int i;
    float sum = 0;
    for(i = 0; i < FFT_SIZE-2; i++){
        sum = sum + dataSet[i];
    }
    return sum / (FFT_SIZE-2);
}


// Condition any packet
float DAD_Calc_conditionPacket(uint8_t* packet, packetType type){
//    switch(type){
//    case()
//    };

    return packet[2];
}


// TODO Condition Humidity data
float DAD_Calc_ConditionHum(uint8_t* packet){
    return packet[2]%100;
}

// TODO Condition Temperature Data
float DAD_Calc_ConditionTemp(uint8_t* packet){
    return packet[2]%120;
}

// TODO Condition Vib Data
float DAD_Calc_ConditionVib(uint8_t* packet){
    return packet[2];
}

// TODO Condition Vib Data
float DAD_Calc_ConditionMic(uint8_t* packet){
    return packet[2];
}
