#include <xlx_definitions.hpp>

/*
    (Multi-tile) CSR SpMV Logic split into four kernels.
        - Reader/Writer
        - Multiplier
        - Partial summer 
        - Accumulator
*/

void accumulate_rows(
        hls::stream<pkt_ind_val>& in_rows,
        hls::stream<indval_t>& out_row,
        const unsigned int tiles,
        const unsigned int runs) {
    
    // XRT 2.15 i.e 2023.1: Pragma conflict happens on 'INLINE' and DATAFLOW pragmas: Inline into dataflow region may break the canonical form.
    // #pragma HLS INLINE
#if DEBUG
    if (DEBUG&1) printf  ("k4::accumulate_rows(): start \n");
#endif
    // TODO: Consider getting rid of the result array alltogether, but at the cost of concurrent R&W ops to HBM
    assert(runs>0);
    runs:
    for (unsigned int h=0; h<runs; h++) {
        #pragma HLS PIPELINE OFF
        // All static variables including arrays are initialized to 0 at COMPILE time if no initial values are provided at declaration
        
        assert(tiles>0);
        tiles:
        for (unsigned int tile=0; tile<tiles; tile++) {
            #pragma HLS PIPELINE OFF
            #pragma HLS LOOP_FLATTEN OFF
            #pragma HLS LOOP_TRIPCOUNT min=(tiles_min) max=(tiles_max)

#if DEBUG
            if (DEBUG&1)  printf  ("k4::accumulate_rows(): start of tile: %d\n", tile);
#endif
        
            #define II 5
            prec_t sum_reg[II];
            #pragma HLS array_partition variable=sum_reg complete dim=0

            for (int k=0; k<II; k++) {
                #define HLS UNROLL
                sum_reg[k] = 0;  
            }   

            indval_t row;
            row_accum:                 
            do {
                #pragma HLS PIPELINE II=1
                #pragma HLS LOOP_TRIPCOUNT min=(rows_min) max=(rows_max) // Not true in the case of emtpy rows though
                pkt_ind_val v = in_rows.read();
                row.get(v.data);

#if DEBUG
                if (DEBUG&2)  printf  ("k4::accumulate_rows(): read row: %d", row.index);
                if (DEBUG&2)  printf  (", val: %f", row.value);
                if (DEBUG&2)  printf  (", last: %d", row.is_last);
                if (DEBUG&2)  printf  (", is_write: %d\n", row.is_write);
#endif

                prec_t prev_sum = 0;
                for (unsigned int k=1; k<II; k++) { // Sum
                    #define HLS UNROLL
                    prev_sum += sum_reg[k];
#if DEBUG
                    if (DEBUG&2)  printf  ("k4::accumulate_rows(): sum_reg[k]: %f\n", sum_reg[k]);
#endif
                    // if (is_curr_interest)  printf  ("k4::accumulate_rows(): sum_reg[k]: %f\n", sum_reg[k]);
                }
                
                for (unsigned int k=0; k<II-1; k++) { // Shift or reset
                    #define HLS UNROLL
                    sum_reg[k] = row.is_write? 0 : sum_reg[k+1];
#if DEBUG
                    if (DEBUG&2)  printf  ("k4::accumulate_rows(): shift_reg[k]: %f\n", sum_reg[k]);
#endif
                    // if (is_curr_interest)  printf  ("k4::accumulate_rows(): shift_reg[k]: %f\n", sum_reg[k]);
                }  
#if DEBUG
                if (DEBUG&2) if (row.is_write) printf  ("k4::accumulate_rows(): row.is_write: %d\n", row.is_write);
                if (DEBUG&2) if (row.is_write) printf  ("k4::accumulate_rows():result[row.index] : %f\n", prev_sum + row.value);
#endif
                // if (is_curr_interest) if (row.is_write) printf  ("k4::accumulate_rows(): row.is_write: %d\n", row.is_write);
                // if (is_curr_interest) if (row.is_write) printf  ("k4::accumulate_rows():result[row.index] : %f\n", prev_sum + row.value);
                
                row.prev_sum = prev_sum;
                if (row.is_write) {
                    out_row.write(row);
                }

                row.value *= !(row.is_write);
                sum_reg[II-1] = sum_reg[0] + row.value; // Put the row in the
            } while (!row.is_last);

            indval_t last {.index=VECTOR_SIZE+BLOCK_SIZE-1, .value=0, .is_last=true, .is_write=false};
            out_row.write(last);
        }
    }
}

void write_results(
    hls::stream<indval_t>& in_rows,
    hls::stream<pkt_block>& out_y,
    const unsigned int y_blocks,
    const unsigned int tiles,
    const unsigned int runs) {
    
    // XRT 2.15 i.e 2023.1: Pragma conflict happens on 'INLINE' and DATAFLOW pragmas: Inline into dataflow region may break the canonical form.
    // #pragma HLS INLINE
#if DEBUG
    if (DEBUG&1) printf  ("k4::write_results(): start \n");
#endif
    // TODO: Consider getting rid of the result array alltogether, but at the cost of concurrent R&W ops to HBM

    prec_t result[VECTOR_SIZE+BLOCK_SIZE]; 
    #pragma HLS ARRAY_PARTITION variable=result type=cyclic factor=8

    for (unsigned int h=0; h<runs; h++) {
        #pragma HLS PIPELINE OFF
        res_init:    
        for (int j=0; j<VECTOR_SIZE+BLOCK_SIZE; j++) {
            #pragma HLS UNROLL factor=16
            #pragma HLS PIPELINE II=1
            result[j] = 0;
        }

        assert(tiles>0);
        tiles:
        for (unsigned int tile=0; tile<tiles; tile++) {
            #pragma HLS PIPELINE OFF
            #pragma HLS LOOP_FLATTEN OFF
            #pragma HLS LOOP_TRIPCOUNT min=(tiles_min) max=(tiles_max)

#if DEBUG
            if (DEBUG&1)  printf  ("k4::write_results(): start of tile: %d\n", tile);
#endif
            indval_t row;
            loc_write: // Warning: This does not take empty row into account!   
            do {
                #pragma HLS PIPELINE II=1
                #pragma HLS DEPENDENCE variable=result type=inter false
                #pragma HLS LOOP_TRIPCOUNT min=100
                row = in_rows.read();
                result[row.index] += row.value + row.prev_sum;
            } while (!row.is_last);
        }
    }

    res_write: 
    for (unsigned int i=0; i<y_blocks; i++) { // The possible trailing buffer is also wrote
        #pragma HLS PIPELINE II=1
        #pragma HLS loop_tripcount min=(rows_blk_min) max=(rows_blk_max)
        valvec_k2k_t res_buffer;
        #pragma HLS array_partition variable=res_buffer.items complete dim=0
#if DEBUG
        if (DEBUG&2)  printf  ("k4::write_results(): res_buffer: %d\n", i);
#endif
        for (unsigned int j=0; j<BLOCK_SIZE; j++) {
            #pragma HLS UNROLL
            res_buffer.items[j] = result[i*BLOCK_SIZE+j];
#if DEBUG
            if (DEBUG&2) std::cout<< res_buffer.items[j] << ", ";
            if (DEBUG&2)  printf  ("%f,", res_buffer.items[j] );
#endif
        }
#if DEBUG
        if (DEBUG&2) std::cout<< std::endl;
        if (DEBUG&2)  printf  ("\n");
#endif
        pkt_block v;
        res_buffer.set(v.data);
        out_y.write(v);
    }

#if DEBUG
    if (DEBUG&1)  printf  ("k4::write_results(): end\n");
#endif

}

extern "C" {

    void csr_spmv_repl_4(
            hls::stream<pkt_block> &out_y,
            hls::stream<pkt_ind_val> &in_rows,
            const unsigned int y_blocks, 
            const unsigned int tiles,
            const unsigned int runs) {
        
        #pragma HLS INTERFACE axis port = out_y
        #pragma HLS INTERFACE axis port = in_rows

        #pragma HLS INTERFACE s_axilite port = y_blocks
        #pragma HLS INTERFACE s_axilite port = tiles
        #pragma HLS INTERFACE s_axilite port = runs


#if DEBUG
        if (DEBUG&1) printf  ("k4::y_blocks: %d\n", y_blocks);
        if (DEBUG&1) printf  ("k4::tiles: %d\n", tiles);
        if (DEBUG&1) printf  ("k4::runs: %d\n", runs);
#endif

        const unsigned int str_depth = 16; //
        static hls::stream<indval_t> row_res;
        #pragma HLS STREAM variable=row_res depth=str_depth 

        #pragma HLS DATAFLOW

        accumulate_rows(in_rows, row_res, tiles, runs);
        write_results(row_res, out_y, y_blocks, tiles, runs);
    }
}

