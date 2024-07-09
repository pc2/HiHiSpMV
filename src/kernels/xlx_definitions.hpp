#pragma once

#include <stdio.h>
#include <stdarg.h>
#include <assert.h> 

#include "ap_axi_sdata.h"
#include "ap_shift_reg.h"
#include "hls_stream.h"
// #include "hls_print.h" // See: https://docs.xilinx.com/r/en-US/ug1399-vitis-hls/HLS-Print-Function

typedef float prec_t;
typedef int prec_int_t;

// Set
#define DEBUG 0 // Turn off before synthesis
#define VECTOR_SIZE 1875 // Square tile side length

#define PREC_SIZE (sizeof(prec_t)*8)
#define INDEX_SIZE (sizeof(int)*8)
#define BURST_SIZE 512
#define BLOCK_SIZE (BURST_SIZE/PREC_SIZE)

// K2K AXI stream types
typedef ap_axiu<1, 0, 0, 0> pkt_sig;
typedef ap_axiu<PREC_SIZE, 0, 0, 0> pkt_atomic;
typedef ap_axiu<BURST_SIZE, 0, 0, 0> pkt_block;
typedef ap_axiu<2*INDEX_SIZE+1, 0, 0, 0> pkt_ind_nnz;
typedef ap_axiu<INDEX_SIZE+PREC_SIZE+1+1, 0, 0, 0> pkt_ind_val;
typedef ap_axiu<2*INDEX_SIZE, 0, 0, 0> pkt_double;
typedef ap_axiu<INDEX_SIZE+16+1+1, 0, 0, 0> pkt_ind_msk;
typedef pkt_ind_nnz pkt_ind_row;

// For reporting trip-count
#define NNZ 2728
const unsigned int rows_min = VECTOR_SIZE;
const unsigned int rows_max = VECTOR_SIZE;
const unsigned int rows_blk_min = (VECTOR_SIZE)/BLOCK_SIZE;
const unsigned int rows_blk_max = (VECTOR_SIZE)/BLOCK_SIZE;
const unsigned int vec_blk_min = (VECTOR_SIZE)/BLOCK_SIZE;
const unsigned int vec_blk_max = (VECTOR_SIZE)/BLOCK_SIZE;
const unsigned int nnz_blk_min = (NNZ)/BLOCK_SIZE;
const unsigned int nnz_blk_max = (NNZ)/BLOCK_SIZE;
const unsigned int tiles_min = BLOCK_SIZE;
const unsigned int tiles_max = BLOCK_SIZE;

// prec_t value based block
typedef struct value_block_type { prec_t items[BLOCK_SIZE]; } valb_t; //TODO: reduce to single type with k2k

// integer block
typedef struct integer_block_type { int items[BLOCK_SIZE]; } intb_t; //TODO: reduce to single type with k2k

// index value pair type
typedef struct index_value_pair {
    int index;
    prec_t value;
    prec_t prev_sum; // Not to be written
    bool is_last;
    bool is_write;

    index_value_pair() = default;

    void get(const ap_uint<INDEX_SIZE+PREC_SIZE+1+1>& d) {
        union {
            unsigned int val_uint;
            int val_int;
        } intuint_t;
        intuint_t.val_uint = d.range(INDEX_SIZE-1, 0);
        index = intuint_t.val_int;
        union {
            unsigned int val_uint;
            prec_t val_fp;
        } intfp_t;
        intfp_t.val_uint = d.range(2*INDEX_SIZE-1, INDEX_SIZE);
        value = intfp_t.val_fp;
        is_last = d.range(INDEX_SIZE+PREC_SIZE, INDEX_SIZE+PREC_SIZE);
        is_write = d.range(INDEX_SIZE+PREC_SIZE+1, INDEX_SIZE+PREC_SIZE+1);
    }

    void set(const ap_uint<INDEX_SIZE+PREC_SIZE+1+1>& d) {
        union {
            unsigned int val_uint;
            int val_int;
        } intuint_t;
        intuint_t.val_int = index;
        d.range(INDEX_SIZE-1, 0) = intuint_t.val_uint;
        union {
            unsigned int val_uint;
            prec_t val_fp;
        } intfp_t;
        intfp_t.val_fp = value;
        d.range(INDEX_SIZE+PREC_SIZE-1, INDEX_SIZE) = intfp_t.val_uint;
        d.range(INDEX_SIZE+PREC_SIZE, INDEX_SIZE+PREC_SIZE) = is_last;
        d.range(INDEX_SIZE+PREC_SIZE+1, INDEX_SIZE+PREC_SIZE+1) = is_write;
    }
} indval_t;

// index index pair type
typedef struct index_index_pair {
    int index;
    int value;
    bool is_last;

    index_index_pair() = default;

    int get(const ap_uint<2*INDEX_SIZE+1>& d) {
        union {
            unsigned int val_uint;
            int val_int;
        } intuint_t;
        intuint_t.val_uint = d.range(INDEX_SIZE-1, 0);
        index = intuint_t.val_int;
        intuint_t.val_uint = d.range(2*INDEX_SIZE-1, INDEX_SIZE);
        value = intuint_t.val_int;
        is_last = d.range(2*INDEX_SIZE, 2*INDEX_SIZE);
        return 0;
    }

    void set(ap_uint<2*INDEX_SIZE+1>& d) {
        union {
            unsigned int val_uint;
            int val_int;
        } intuint_t;
        intuint_t.val_int = index;
        d.range(INDEX_SIZE-1, 0) = intuint_t.val_uint;
        intuint_t.val_int = value;
        d.range(2*INDEX_SIZE-1, INDEX_SIZE) = intuint_t.val_uint;
        d.range(2*INDEX_SIZE, 2*INDEX_SIZE) = is_last;
    }

} indind_t;

// index index pair type
typedef struct index_block_mask_pair {
    int index;
    ap_uint<16> mask = 0x0; // Mask for the block
    bool is_write;
    bool is_last;
    bool is_blk_read;

    index_block_mask_pair() = default;

    int get(const ap_uint<INDEX_SIZE+16+1+1>& d) {
        union {
            unsigned int val_uint;
            int val_int;
        } intuint_t;
        intuint_t.val_uint = d.range(INDEX_SIZE-1, 0);
        index = intuint_t.val_int;
        mask = d.range(INDEX_SIZE+16-1, INDEX_SIZE);
        is_write = d.range(INDEX_SIZE+16+1-1, INDEX_SIZE+16);
        is_last = d.range(INDEX_SIZE+16+1+1-1, INDEX_SIZE+16+1);
        return 0;
    }

    void set(ap_uint<INDEX_SIZE+16+1+1>& d) {
        union {
            unsigned int val_uint;
            int val_int;
        } intuint_t;
        intuint_t.val_int = index;
        d.range(INDEX_SIZE-1, 0) = intuint_t.val_uint;
        d.range(INDEX_SIZE+16-1, INDEX_SIZE) = mask;
        d.range(INDEX_SIZE+16+1-1, INDEX_SIZE+16) = is_write;
        d.range(INDEX_SIZE+16+1+1-1, INDEX_SIZE+16+1) = is_last;
    }

} indmsk_t;

// valb_t for k2k streaming type; exists because of ap_uint support only for k2k streaming
typedef struct value_vector_k2k_type {
    prec_t items[BLOCK_SIZE];
    value_vector_k2k_type() = default;
    
    int get(const ap_uint<BURST_SIZE>& d) {
        for (int i=0; i<BURST_SIZE; i+=PREC_SIZE) {
            #pragma HLS UNROLL
            union {
                unsigned int val_uint;
                prec_t val_fp;
            } intfp_t;
            intfp_t.val_uint = d.range(i+PREC_SIZE-1, i);
            items[i/PREC_SIZE] = intfp_t.val_fp;
            // std::cout<< std::setprecision(10) << "valvec_k2k_t::get(): i: "<< i << ", value: " << (float) intfp_t.val_fp << std::endl;
        }
        return 0;
    }

    void set(ap_uint<BURST_SIZE>& d) {
        for (int i=0; i<BURST_SIZE; i+=PREC_SIZE) {
            #pragma HLS UNROLL
            union {
                unsigned int val_uint;
                prec_t val_fp;
            } intfp_t;
            intfp_t.val_fp = items[i/PREC_SIZE];
            d.range(i+PREC_SIZE-1, i) = intfp_t.val_uint;
            // std::cout<< std::setprecision(10) << "valvec_k2k_t::set(): i: "<< i << ", value: " << (float) intfp_t.val_fp << std::endl;
        }
    }
} valvec_k2k_t; //TODO: reduce to single type with k2k

// intb_t for k2k streaming type; exists because of ap_uint support only for k2k streaming
typedef struct index_vector_k2k_type {
    int items[BLOCK_SIZE];
    index_vector_k2k_type() = default;
    
    void get(const ap_uint<BURST_SIZE>& d) {
        for (int i=0; i<BURST_SIZE; i+=PREC_SIZE) {
            #pragma HLS UNROLL
            union {
                unsigned int val_uint;
                int val_int;
            } intfp_t;
            intfp_t.val_uint = d.range(i+PREC_SIZE-1, i);
            items[i/PREC_SIZE] = intfp_t.val_int;
            // std::cout<<"indvec_k2k_t::get(): i: "<< i << ", value: " << intfp_t.val_int << std::endl;
        }
    }

    void set(ap_uint<BURST_SIZE>& d) {
        for (int i=0; i<BURST_SIZE; i+=PREC_SIZE) {
            #pragma HLS UNROLL
            union {
                unsigned int val_uint;
                int val_int;
            } intfp_t;
            intfp_t.val_int = items[i/PREC_SIZE];
            d.range(i+PREC_SIZE-1, i) = intfp_t.val_uint;
            // std::cout<<"indvec_k2k_t::set(): i: "<< i << ", value: " << intfp_t.val_int << std::endl;
        }
    }

} indvec_k2k_t; //TODO: reduce to single type with k2k