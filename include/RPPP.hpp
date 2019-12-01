#pragma once
#include <array>
#include <vector>
#include <queue>
#include <limits>
#include <algorithm>

namespace rppp{

    #define MID ((lo + hi + 1) / 2)

    constexpr uint64_t sqrt_helper(uint64_t x, uint64_t lo, uint64_t hi)
    {
    return lo == hi ? lo : ((x / MID < MID)
        ? sqrt_helper(x, lo, MID - 1) : sqrt_helper(x, MID, hi));
    }
    constexpr uint64_t ct_sqrt(uint64_t x)
    {
    return sqrt_helper(x, 0, x / 2 + 1);
    }
    constexpr bool is_prime(int num)
    {
        if (num < 2) return false;
        else if (num == 2) return true;
        else if (num % 2 == 0) return false;

        double sqrtNum = ct_sqrt(num);
        for (int i = 3; i <= sqrtNum; i += 2)
        {
            if (num % i == 0)
            {
                return false;
            }
        }
        return true;
    }
    constexpr int multi_ceil(int n, int m){
        return (n+m-1)/m*m;
    }
    constexpr int multi_floor(int n, int m){
        return n/m*m;
    }

    enum Status{
        OK,
        OK_PARITY_GENERATED,
        NO_ELEMENT,
    };

    using seq_id_t = uint16_t;
    struct Header{
        seq_id_t seq_id;
    };

    template<class T, int block_num>
    struct StreamData{
        Header header;
        uint8_t data[multi_ceil(sizeof(T), block_num)]; 
        // (e.g. if block_num==4 then 6->8, 21->24, 16->16)
    };

    template<class T, int parity_size, size_t bytes = multi_ceil(sizeof(T), parity_size)>
    class EncodeBuffer{
        static_assert(is_prime(parity_size+1), "n + 1 is must be prime.");
        static_assert(parity_size >= 2, "n must be >= 2.");
        static_assert(std::is_pod<T>::value, "T must be a POD type.");
        using Block = std::array<uint8_t, bytes/parity_size>;
        using Blocks = std::array<Block, parity_size>;
        using Stream = std::pair<Header, Blocks>;
        std::vector<Blocks> m_inBuf;
        std::queue<Stream> m_outBuf;
        seq_id_t m_seqId;

    public:
        EncodeBuffer() : m_seqId(0){}

        Status enq(const T &item){
            Blocks blocks {};
            memcpy(blocks.data(), &item, sizeof(T));

            m_inBuf.push_back(blocks);
            push2outbuf(blocks);

            if (m_inBuf.size() == parity_size){
                // P parity
                // horizonal parity
                Blocks p {};
                for (int i=0; i<parity_size; i++){
                    for (int j=0; j<parity_size; j++){                   
                        p[j] = block_xor(p[j], m_inBuf[i][j]);
                    }
                }
                push2outbuf(p);

                // Q parity
                // diagonal parity
                /*
                example: parity_size = 4

                a b c d p  q
                ---------- -
                0 1 2 3    0
                  0 1 2 3  1
                3   0 1 2  2
                2 3   0 1  3

                q0 = a0 xor b0 xor c0 xor d0
                q1 = b1 xor c1 xor d1 xor p1
                */
                m_inBuf.push_back(p);
                Blocks q {};
                for (int j=0; j<parity_size; j++){
                    for (int i=0; i<parity_size; i++){              
                        q[j] = block_xor(q[j], m_inBuf[(i+j)%(parity_size+1)][i]);
                    }
                }
                push2outbuf(q);
                
                m_inBuf.clear();
                return Status::OK_PARITY_GENERATED;
            }
            return Status::OK;
        }

        Status deq(StreamData<T, parity_size>* psd){
            if(m_outBuf.size() == 0)
                return Status::NO_ELEMENT;
            
            psd->header = m_outBuf.front().first;
            memcpy(&(psd->data), m_outBuf.front().second.data(), bytes);
            m_outBuf.pop();
            
            return Status::OK;
        }

        void reset(){
            m_inBuf.clear();
            std::queue<Stream> empty;
            std::swap(empty, m_outBuf);
            m_seqId = 0;
        }

        size_t count(){
            return m_outBuf.size();
        }

    private:
        inline void push2outbuf(Blocks blocks){
            Header h;
            h.seq_id = m_seqId;
            m_seqId++;
            if (m_seqId == multi_floor(std::numeric_limits<seq_id_t>::max(), parity_size+2))
                m_seqId = 0;

            Stream st;
            st.first = h;
            st.second = blocks;
            m_outBuf.push(st);
        }
        inline Block block_xor(const Block& lhs, const Block& rhs){
            Block ret;
            for(int i=0; i<static_cast<int>(ret.size()); i++)
                ret[i] = lhs[i] xor rhs[i];
            return ret;
        }
    };

    template<class T, int parity_size, size_t bytes = multi_ceil(sizeof(T), parity_size)>
    class DecodeBuffer{
        static_assert(is_prime(parity_size+1), "n + 1 must be prime.");
        static_assert(parity_size >= 2, "n must be >= 2.");
        static_assert(std::is_pod<T>::value, "T must be a POD type.");
        using Block = std::array<uint8_t, bytes/parity_size>;
        using Blocks = std::array<Block, parity_size>;
        using Stream = std::pair<Header, Blocks>;
        std::vector<Stream> m_inBuf;
        std::queue<Blocks> m_outBuf;
        seq_id_t m_expectSeqId;
        seq_id_t m_nextSeqIdFloor;
        seq_id_t m_prevSeqId;
        uint8_t m_lossCnt;
        bool m_flag_first_call;

    public:
        DecodeBuffer() :
            m_expectSeqId(0),
            m_nextSeqIdFloor(parity_size+2),
            m_prevSeqId(0),
            m_lossCnt(0),
            m_flag_first_call(true)
        {}

        Status enq(const StreamData<T, parity_size> &sd){
            Stream stream;
            stream.first = sd.header;
            memcpy(stream.second.data(), sd.data, sizeof(sd.data));

            // for encoder's reset
            if (stream.first.seq_id <= m_prevSeqId && not m_flag_first_call)
            {
                m_inBuf.clear();
                m_expectSeqId = multi_floor(stream.first.seq_id, parity_size+2);
                m_nextSeqIdFloor = multi_ceil(m_expectSeqId+1, parity_size+2);
                m_prevSeqId = stream.first.seq_id;
                m_lossCnt = 0;
                m_flag_first_call = true;
                enq(sd);
            }
            if (m_flag_first_call)
            {
                m_flag_first_call = false;
            }

            if(stream.first.seq_id == m_expectSeqId) // no lost
            {
                m_inBuf.push_back(stream);
                if (m_lossCnt == 0)
                    m_outBuf.push(stream.second);
                m_expectSeqId = stream.first.seq_id+1;
            }
            else if (stream.first.seq_id < m_expectSeqId) // expired sequence id
            {
                // NO OPERATION
            }
            else if(stream.first.seq_id > m_expectSeqId) // skipped sequence id
            {
                if(stream.first.seq_id >= m_nextSeqIdFloor) // next parity set
                {
                    next_period();
                    enq(sd);
                }
                else if(stream.first.seq_id == m_expectSeqId + 1) // lost 1
                {
                    m_inBuf.push_back(stream);
                    m_lossCnt += 1;
                    m_expectSeqId = stream.first.seq_id+1;
                }
                else if(stream.first.seq_id == m_expectSeqId + 2) // lost 2
                {
                    m_inBuf.push_back(stream);
                    m_lossCnt += 2;
                    m_expectSeqId = stream.first.seq_id+1;
                }
                else // lost >= 3
                {
                    m_lossCnt += 3;
                }
            }

            if (m_lossCnt >= 3)
            {
                next_period();
            }
            else if (m_inBuf.size() == parity_size)
            {
                decode();
                next_period();
            }
            m_prevSeqId = stream.first.seq_id;

            return Status::OK;
        }

        Status deq(T *p){
            if(m_outBuf.size() == 0)
                return Status::NO_ELEMENT;
            
            memcpy(p, m_outBuf.front().data(), sizeof(T));
            m_outBuf.pop();
            
            return Status::OK;
        }

        void reset(){
            m_inBuf.clear();
            std::queue<Blocks> empty;
            std::swap(empty, m_outBuf);
            m_expectSeqId = 0;
            m_nextSeqIdFloor = parity_size+2;
            m_prevSeqId = 0;
            m_lossCnt = 0;
            m_flag_first_call = true;
        }

        size_t count(){
            return m_outBuf.size();
        }
    
    private:
        inline void next_period(){
            m_inBuf.clear();
            m_lossCnt=0;
            m_expectSeqId = m_nextSeqIdFloor;
            m_nextSeqIdFloor = multi_ceil(m_expectSeqId+1, parity_size+2);
        }

        inline void decode(){
            if (m_lossCnt == 0)
            {
                return;
            }
            else if (m_lossCnt == 1) // calculate from Horizaonal parity
            {
                int i;
                for (i=0; i<parity_size; i++) 
                {
                    if(m_inBuf[i].first.seq_id%(parity_size+2) != i)
                    {
                        Blocks restore_data {};
                        for (int j=0; j<parity_size; j++)
                        {
                            for (int k=0; k<parity_size; k++)
                            {
                                restore_data[j] = block_xor(restore_data[j], m_inBuf[k].second[j]);
                            }
                        }
                        m_outBuf.push(restore_data);
                        break;
                    }
                }
                for (;i<parity_size-1; i++)
                { // last of m_inBuf is Horizonal Parity Blocks
                    m_outBuf.push(m_inBuf[i].second);
                }
            }
            else // m_lossCnt == 2, calculate from Diagonal & Hrizonal Parity
            {
                std::vector<Blocks> all_data;
                std::vector<int> drop_numbers;

                // init all_data and drop_number
                int k = 0;
                for (int i=0; i<parity_size; i++)
                {
                    while(m_inBuf[i].first.seq_id%(parity_size+2) != k){
                        Blocks b {};
                        all_data.push_back(b);
                        drop_numbers.push_back(k);
                        k++;
                    }
                    all_data.push_back(m_inBuf[i].second);
                    k++;
                }

                // scanning blocks what can be decoded from Diagonal Parity
                std::array<int, parity_size+1> q_count {};
                for(auto i : drop_numbers)
                {
                    for(int j=0; j<parity_size; j++)
                    {
                        if(q_number(i,j) != parity_size)
                            q_count[q_number(i,j)] += 1;
                        else
                            q_count[q_number(i,j)] = 0;
                    }
                }

                // decode blocks what can be decoded from Diagonal Parity (q_count[q_number(i,j)] == 1)
                int decode_count = 0;
                std::vector<int> calculatable_row;
                for(int i=0; i<parity_size; i++)
                    calculatable_row.push_back(i);
                while (decode_count < parity_size*2){
                    for(auto i : drop_numbers)
                    {
                        for(int j : calculatable_row)
                        {
                            if(q_count[q_number(i,j)] == 1) // It is can be decoded from Diagonal Parity
                            {
                                // decode
                                Block block {};
                                for(int k=0; k<parity_size+1; k++)
                                {
                                    if((j+k)%(parity_size+1) != parity_size)
                                        block = block_xor(block, all_data[(i+k)%(parity_size+1)][(j+k)%(parity_size+1)]);
                                }
                                block = block_xor(block, all_data[parity_size+1][q_number(i,j)]); // xor Diagonal parity

                                q_count[q_number(i,j)] -= 1; // set a next decodable block
                                all_data[i][j] = block;
                                decode_count++;

                                // decode neighboor block
                                Block neighboor_block {};
                                int max = *std::max_element(drop_numbers.begin(), drop_numbers.end());
                                int min = *std::min_element(drop_numbers.begin(), drop_numbers.end());
                                int neighboor_i = (i==max)?min:max;

                                for(int k=0; k<parity_size+1; k++)
                                {
                                    neighboor_block = block_xor(neighboor_block, all_data[k][j]);
                                }

                                q_count[q_number(neighboor_i,j)] -= 1; // set a next decodable block
                                all_data[neighboor_i][j] = neighboor_block;
                                decode_count++;

                                calculatable_row.erase(
                                    std::remove(calculatable_row.begin(), calculatable_row.end(), j), calculatable_row.end()
                                );
                            }
                        }
                    }
                }
                // output remain data
                for(int i=*std::min_element(drop_numbers.begin(), drop_numbers.end()); i<parity_size; i++)
                {
                    m_outBuf.push(all_data[i]);
                }
            }
        }

        inline Block block_xor(const Block& lhs, const Block& rhs){
            Block ret;
            for(int i=0; i<static_cast<int>(ret.size()); i++)
                ret[i] = lhs[i] xor rhs[i];
            return ret;
        }

        inline int q_number(int i, int j){
            // Diagonal parity block position where block_i_j is calculated
            return ((i-j)+parity_size+1)%(parity_size+1);
        }
    };
}