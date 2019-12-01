#include "RPPP.hpp"
#include <iostream>

struct Position{
    int x;
    int y;
    int z;
};
struct Rotation{
    int x;
    int y;
    int z;
};
struct Scale{
    int x;
    int y;
    int z;
};

// the data what you want to send.
struct SampleNetVar{
    Position pos;
    Rotation rot;
    Scale sca;
    float health;
    uint16_t id;
};

void update(SampleNetVar &s){
    // some code for update a var.
}
void use(SampleNetVar &s){
    // some code for use a var.
}

void send(const void *data, size_t size){
    // some code for send.
}

void receive(const void *data, size_t size){
    // some code for receive.
}

void sender(){
    SampleNetVar send_var;
    rppp::EncodeBuffer<SampleNetVar, 10> encoder;
    rppp::StreamData<SampleNetVar, 10> stream_data;

    // ENCODER
    for(int i=0; i<100; i++){
        update(send_var);
        encoder.enq(send_var);
        while (encoder.deq(&stream_data) == rppp::Status::OK)
            send(&stream_data, sizeof(stream_data));
    }
}

void receiver(){
    SampleNetVar receive_var;
    rppp::DecodeBuffer<SampleNetVar, 10> decoder;
    rppp::StreamData<SampleNetVar, 10> stream_data;

    // DECODER
    for(;;){
        receive(&stream_data, sizeof(stream_data));
        decoder.enq(stream_data);
        while (decoder.deq(&receive_var) == rppp::Status::OK)
            use(receive_var);   
    }
}