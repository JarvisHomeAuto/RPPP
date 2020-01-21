#include "gtest/gtest.h"
#include "RPPP.hpp"
#include <vector>
#include <array>
#include <bitset>
#include <cstring>

using namespace rppp;

struct randomdata{
        char random[100];
} randomdata;

class RPPPTest : public ::testing::Test {

protected:
    struct NetVar0{
        int x;
        int y;
        uint8_t z;
        uint16_t a;

        bool operator==(const NetVar0 &other) const{
            return (x == other.x) && (y == other.y) && (z == other.z) && (a == other.a);
        }
    };
    
    struct NetVar1{
        int x;
        int y;
        uint8_t z;
        uint16_t a;
        double k;
        double c;

        bool operator==(const NetVar1 &other) const{
            return (x == other.x) && (y == other.y) && (z == other.z) && (a == other.a) && (k == other.k) && (c == other.c);
        }
    };

    virtual void SetUp() {      
        for (size_t i=0; i<sizeof(randomdata); i++)
            randomdata.random[i] = i;  
    };

    virtual void TearDown() {
    };

    template<typename T, int parity_size>
    struct encode_simple_test{
    void operator()(){
        std::cout << typeid(T).name() << " " << parity_size << std::endl;
        EncodeBuffer<T, parity_size> e_buf;

        T in;
        EXPECT_EQ(e_buf.count(), 0);
        EXPECT_EQ(e_buf.enq(in), Status::OK);
        EXPECT_EQ(e_buf.count(), 1);
        StreamData<T, parity_size> pipe;
        EXPECT_EQ(e_buf.deq(&pipe), Status::OK);
        EXPECT_EQ(e_buf.count(), 0);

        // sequence id test
        EXPECT_EQ(pipe.header.seq_id, 0);

        // non-parity data test
        EXPECT_EQ(memcmp(&in, &pipe.data, sizeof(in)), 0);
    }
    };

    template<typename T, int parity_size>
    struct encode_logic_test{
    void operator()(){
        std::cout << typeid(T).name() << " " << parity_size << std::endl;
        auto bytes = ((sizeof(T)+(parity_size-1))/parity_size)*parity_size;
        EncodeBuffer<T, parity_size> e_buf;

        // prepare data
        std::array<T,parity_size> in;
        for (size_t i=0; i<in.size(); i++){
            memcpy(&in[i], randomdata.random + i, sizeof(T));
        }

        // enqueue encode buffer
        EXPECT_EQ(e_buf.count(), 0);
        for (int i=0; i<parity_size-1; i++)
            EXPECT_EQ(e_buf.enq(in[i]), Status::OK);
        EXPECT_EQ(e_buf.count(), parity_size-1);
        EXPECT_EQ(e_buf.enq(in[parity_size-1]), Status::OK_PARITY_GENERATED);
        EXPECT_EQ(e_buf.count(), parity_size+2);

        // dequeue encode buffer
        std::array<StreamData<T, parity_size>, parity_size+2> pipe;
        for (int i=0; i<parity_size+2; i++)
            EXPECT_EQ(e_buf.deq(&pipe[i]), Status::OK);
        EXPECT_EQ(e_buf.count(), 0);
        EXPECT_EQ(e_buf.deq(&pipe[0]), Status::NO_ELEMENT);
        EXPECT_EQ(e_buf.count(), 0);

        // sequence id test
        for (int i=0; i<parity_size+2; i++)
            EXPECT_EQ(pipe[i].header.seq_id, i);

        // non-parity data test
        for (int i=0; i<parity_size; i++)
            EXPECT_EQ(memcmp(&in[i], &pipe[i].data, sizeof(in[i])), 0);

        // horizonal parity test
        for(size_t i=0; i<bytes; i++){
            uint8_t data = 0;
            for (int k=0; k<parity_size; k++) data ^= pipe[k].data[i];
            EXPECT_EQ(pipe[parity_size].data[i], data);
        }

        // diagonal parity test
        auto block_bytes = bytes/parity_size;
        EXPECT_EQ(sizeof(pipe[0].data), block_bytes*parity_size);

        for (int i=0; i<parity_size; i++){
            for(size_t j=0; j<block_bytes; j++){
                uint8_t q = pipe[parity_size+1].data[i*block_bytes+j];
                uint8_t cmp = 0;
                for (int k=0; k<parity_size; k++)
                    cmp ^= pipe[(i+k)%(parity_size+1)].data[k*block_bytes+j];
                EXPECT_EQ(q, cmp);
            }
        }
    }
    };

    template<typename T, int parity_size>
    struct decode_simple_test{
    void operator()(){
        std::cout << typeid(T).name() << " " << parity_size << std::endl;
        DecodeBuffer<T, parity_size> d_buf;
        StreamData<T, parity_size> in;
        in.header.seq_id = 0;
        for(size_t i=0; i<sizeof(in.data); i++){
            in.data[i] = i;
        }
        EXPECT_EQ(d_buf.count(), 0);
        EXPECT_EQ(d_buf.enq(in), Status::OK);
        EXPECT_EQ(d_buf.count(), 1);
        T out;
        EXPECT_EQ(d_buf.deq(&out), Status::OK);
        EXPECT_EQ(d_buf.count(), 0);

        EXPECT_EQ(memcmp(in.data, &out, sizeof(T)), 0);
    }
    };

    template<typename T, int parity_size>
    struct decode_non_drop_test{
    void operator()(){
        std::cout << typeid(T).name() << " " << parity_size << std::endl;
        DecodeBuffer<T, parity_size> d_buf;

        // prepare data
        std::array<StreamData<T, parity_size>, parity_size+2> in;
        for (size_t i=0; i<in.size(); i++){
            in[i].header.seq_id = i;
            memcpy(in[i].data, randomdata.random + i, sizeof(T));
        }

        // enqueue decode buffer
        EXPECT_EQ(d_buf.count(), 0);
        for (int i=0; i<parity_size; i++){
            EXPECT_EQ(d_buf.enq(in[i]), Status::OK);
            EXPECT_EQ(d_buf.count(), i+1);
        }

        EXPECT_EQ(d_buf.enq(in[parity_size]), Status::OK);
        EXPECT_EQ(d_buf.count(), parity_size);
        EXPECT_EQ(d_buf.enq(in[parity_size+1]), Status::OK);
        EXPECT_EQ(d_buf.count(), parity_size);

        // dequeue decode buffer
        std::array<T, parity_size> out;
        for (int i=0; i<parity_size; i++){
            EXPECT_EQ(d_buf.deq(&out[i]), Status::OK);
            EXPECT_EQ(d_buf.count(), (parity_size-1-i));
        }
        EXPECT_EQ(d_buf.deq(&out[0]), Status::NO_ELEMENT);

        // data test
        for (int i=0; i<parity_size; i++)
            EXPECT_EQ(memcmp(in[i].data, &out[i], sizeof(T)), 0);
    }
    };

    template<typename T, int parity_size>
    struct drop_test{
    void operator()(){
        std::cout << typeid(T).name() << " " << parity_size << std::endl;
        EncodeBuffer<T, parity_size> e_buf;
        DecodeBuffer<T, parity_size> d_buf;

        // prepare data
        std::array<T,parity_size> in;
        for (size_t i=0; i<in.size(); i++){
            memcpy(&in[i], randomdata.random + i, sizeof(T));
        }

        // enqueue encode buffer
        EXPECT_EQ(e_buf.count(), 0);
        for (int i=0; i<parity_size-1; i++)
            EXPECT_EQ(e_buf.enq(in[i]), Status::OK);
        EXPECT_EQ(e_buf.count(), parity_size-1);
        EXPECT_EQ(e_buf.enq(in[parity_size-1]), Status::OK_PARITY_GENERATED);
        EXPECT_EQ(e_buf.count(), parity_size+2);

        // dequeue encode buffer
        std::array<StreamData<T, parity_size>, parity_size+2> pipe;
        for (int i=0; i<parity_size+2; i++)
            EXPECT_EQ(e_buf.deq(&pipe[i]), Status::OK);
        EXPECT_EQ(e_buf.count(), 0);
        EXPECT_EQ(e_buf.deq(&pipe[0]), Status::NO_ELEMENT);
        EXPECT_EQ(e_buf.count(), 0);

        /* ~ SOME BAD PIPE ~ */

        EXPECT_EQ(d_buf.count(), 0);

        
        for (int drop1=0;drop1<parity_size+2;drop1++){
            
            std::cout << "drop: " << drop1 << std::endl;
            // lost 1 test
            for (int i=0; i<parity_size+2; i++){
                if(i!=drop1)
                    EXPECT_EQ(d_buf.enq(pipe[i]), Status::OK);
                else
                    continue;
            }
            EXPECT_EQ(d_buf.count(), parity_size);

            std::array<T, parity_size> out;
            for (int i=0; i<parity_size; i++){
                EXPECT_EQ(d_buf.deq(&out[i]), Status::OK);
                EXPECT_EQ(d_buf.count(), (parity_size-1-i));
            }
            for (int i=0; i<parity_size; i++){
                EXPECT_EQ(in[i],out[i]);
            }
            d_buf.reset();
            EXPECT_EQ(d_buf.count(), 0);
            
            
            for (int drop2=drop1+1;drop2<parity_size+2;drop2++){
                std::cout << "drop: " << drop1 << " " << drop2 << std::endl;
                // lost 2 test
                for (int i=0; i<parity_size+2; i++){
                    if((i!=drop1) and (i!=drop2))
                        EXPECT_EQ(d_buf.enq(pipe[i]), Status::OK);
                    else
                        continue;
                }
                EXPECT_EQ(d_buf.count(), parity_size);

                std::array<T, parity_size> out;
                for (int i=0; i<parity_size; i++){
                    EXPECT_EQ(d_buf.deq(&out[i]), Status::OK);
                    EXPECT_EQ(d_buf.count(), (parity_size-1-i));
                }
                
                for (int i=0; i<parity_size; i++){
                    EXPECT_EQ(in[i],out[i]);
                }
                d_buf.reset();
            }
            
        }
        
    }
    };

    template<typename T, int parity_size>
    struct encode_boundary_seq_id_test{
    void operator()(){
        std::cout << typeid(T).name() << " " << parity_size << std::endl;
        EncodeBuffer<T, parity_size> e_buf;
        T in {};
        StreamData<T, parity_size> pipe {};
        for (int i=0; i<std::numeric_limits<seq_id_t>::max()+2; i++){
            e_buf.enq(in);
        }
        e_buf.deq(&pipe);
        auto prev_seq_id = pipe.header.seq_id;
        for (int i=0; i<std::numeric_limits<seq_id_t>::max()+2; i++){
            e_buf.deq(&pipe);
            if(pipe.header.seq_id == 0){
                EXPECT_EQ(prev_seq_id%(parity_size+2), parity_size+1);
                break;
            }
            prev_seq_id = pipe.header.seq_id;
        }
    }
    };

    template<typename T, int parity_size>
    struct decode_boundary_seq_id_test{
    void operator()(){
        std::cout << typeid(T).name() << " " << parity_size << std::endl;
        DecodeBuffer<T, parity_size> d_buf;
        StreamData<T, parity_size> pipe {};

        // enqueue some data
        EXPECT_EQ(d_buf.count(), 0);
        for (int i=0; i<10; i++){
            for (int j=0; j<parity_size+2; j++){
                pipe.header.seq_id = i*(parity_size+2) + j;
                d_buf.enq(pipe);
            }
            EXPECT_EQ(d_buf.count(), parity_size*(i+1));
        }

        // push normal data
        for (int j=0; j<parity_size; j++){
            pipe.header.seq_id = (parity_size+2)*10 + j;
            d_buf.enq(pipe);
        }
        EXPECT_EQ(d_buf.count(), parity_size*11);
        // push parity data
        for (int j=parity_size; j<parity_size+2; j++){
            pipe.header.seq_id = (parity_size+2)*10 + j;
            d_buf.enq(pipe);
        }
        EXPECT_EQ(d_buf.count(), parity_size*11);

        // push normal data
        for (int j=0; j<parity_size; j++){
            pipe.header.seq_id = (parity_size+2)*11 + j;
            d_buf.enq(pipe);
        }
        EXPECT_EQ(d_buf.count(), parity_size*12);
        // push lower seq_id data
        for (int j=0; j<2; j++){
            pipe.header.seq_id = j;
            d_buf.enq(pipe);
        }
        EXPECT_EQ(d_buf.count(), parity_size*12 + 2);

        d_buf.reset();
        for(int i=0; i<1; i++){
            pipe.header.seq_id = 0;
            d_buf.enq(pipe);
            EXPECT_EQ(d_buf.count(), i+1);
        }   
    }
    };

    template<template<typename T, int parity_size> typename test_func>
    void tester(){
        test_func<NetVar0, 2>()();
        test_func<NetVar0, 4>()();
        test_func<NetVar0, 6>()();
        test_func<NetVar0, 10>()();
        test_func<NetVar0, 12>()();
        test_func<NetVar0, 16>()();

        test_func<NetVar1, 2>()();
        test_func<NetVar1, 4>()();
        test_func<NetVar1, 6>()();
        test_func<NetVar1, 10>()();
        test_func<NetVar1, 12>()();
        test_func<NetVar1, 16>()();
    }
};

TEST_F(RPPPTest, encode_simple_test){
    tester<encode_simple_test>();
}

TEST_F(RPPPTest, encode_logic_test){
    tester<encode_logic_test>();
}

TEST_F(RPPPTest, decode_simple_test){
    tester<decode_simple_test>();
}

TEST_F(RPPPTest, decode_non_drop_test){
    tester<decode_non_drop_test>();
}

TEST_F(RPPPTest, drop_restoration_test) {
    tester<drop_test>();
}

TEST_F(RPPPTest, encode_boundary_seq_id_test) {
    tester<encode_boundary_seq_id_test>();
}

TEST_F(RPPPTest, decode_boundary_seq_id_test) {
    tester<decode_boundary_seq_id_test>();
}

TEST_F(RPPPTest, buf_reset_test){
    EncodeBuffer<NetVar0, 4> e_buf;
    DecodeBuffer<NetVar0, 4> d_buf;

    NetVar0 in {};
    StreamData<NetVar0, 4> pipe {};

    EXPECT_EQ(e_buf.enq(in), Status::OK);
    EXPECT_EQ(e_buf.enq(in), Status::OK);
    EXPECT_EQ(e_buf.enq(in), Status::OK);
    EXPECT_EQ(e_buf.deq(&pipe), Status::OK);
    EXPECT_EQ(e_buf.deq(&pipe), Status::OK);
    EXPECT_EQ(e_buf.count(), 1);
    EXPECT_EQ(pipe.header.seq_id, 1);

    e_buf.reset();
    EXPECT_EQ(e_buf.count(), 0);
    EXPECT_EQ(e_buf.enq(in), Status::OK);
    EXPECT_EQ(e_buf.deq(&pipe), Status::OK);
    EXPECT_EQ(pipe.header.seq_id, 0);

    pipe.header.seq_id = 0;
    EXPECT_EQ(d_buf.enq(pipe), Status::OK);
    pipe.header.seq_id = 1;
    EXPECT_EQ(d_buf.enq(pipe), Status::OK);
    EXPECT_EQ(d_buf.count(), 2);
    d_buf.reset();
    EXPECT_EQ(d_buf.count(), 0);
    pipe.header.seq_id = 0;
    EXPECT_EQ(d_buf.enq(pipe), Status::OK);
    EXPECT_EQ(d_buf.count(), 1);
}