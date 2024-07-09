#include <xlx_definitions.hpp>

/*
    (Multi-tile) CSR SpMV Logic split into four kernels.
        - Reader/Writer
        - Multiplier
        - Partial summer 
        - Accumulator
*/

void read_indices(
        hls::stream<pkt_block> &out_indices,
        const intb_t* indices,
        const unsigned int ind_end_1,
        const unsigned int ind_end_2,
        const unsigned int runs) {
    
    unsigned int ind_end = ind_end_1 + ind_end_2 + 1; // +1: nnzs in each block
    // XRT 2.15 i.e 2023.1: Pragma conflict happens on 'INLINE' and DATAFLOW pragmas: Inline into dataflow region may break the canonical form.
    #pragma HLS INLINE
#if DEBUG 
    if (DEBUG&1) printf ("k1::read_indices(): start\n");
#endif
    assert(runs>0); // Helps inferring the compiler that the loop must be entered at least
    for (unsigned int h=0; h<runs; h++)  {
        #pragma HLS PIPELINE OFF

        assert(ind_end>0); // Helps inferring the compiler that the loop must be entered at least
        for (unsigned int i=0; i<ind_end; i++) {
            #pragma HLS PIPELINE II=1
            #pragma HLS loop_tripcount min=(rows_blk_min+nnz_blk_min) max=(rows_blk_max+nnz_blk_max)
            intb_t ind_block = indices[i];
            indvec_k2k_t ind_buff; // TODO: Combine the structs
#if DEBUG 
            if (DEBUG&2) printf ("k1::index-block: %d\n", i);
#endif
            for (unsigned int j=0; j<BLOCK_SIZE; j++) { // Auto unrolled
                #pragma HLS UNROLL
                ind_buff.items[j] = ind_block.items[j];
#if DEBUG 
                if (DEBUG&2) printf ("%d,", ind_block.items[j]);
#endif
            }
#if DEBUG 
            if (DEBUG&2) printf ("\n");
#endif
            pkt_block v;
            ind_buff.set(v.data);
            out_indices.write(v);
        }
    }
    
#if DEBUG 
    if (DEBUG&1) printf ("k1::read_indices(): end\n");
#endif
}

void read_values(
        hls::stream<pkt_block> &out_values,
        const valb_t* values,
        const unsigned int vals_end_1,
        const unsigned int vals_end_2,
        const unsigned int runs) {
    
    unsigned int vals_end = vals_end_1 + vals_end_2;

    #pragma HLS INLINE
#if DEBUG 
    if (DEBUG&1) printf ("k1::read_values(): start\n");
#endif
    assert(runs>0); // Helps inferring the compiler that the loop must be entered at least
    for (unsigned int h=0; h<runs; h++) {
        #pragma HLS PIPELINE OFF
        
        assert(vals_end>0); // Helps inferring the compiler that the loop must be entered at least
        for (unsigned int i=0; i<vals_end; i++) {
            #pragma HLS PIPELINE II=1
            #pragma HLS loop_tripcount min=(vec_blk_min+nnz_blk_min) max=(vec_blk_max+nnz_blk_max)
            valb_t val_block = values[i];
            valvec_k2k_t vals_buff; // TODO: Combine the structs
#if DEBUG 
            if (DEBUG&2) printf ("k1::value-block: %d\n", i);
#endif
            for (unsigned int j=0; j<BLOCK_SIZE; j++) { // Auto unrolled
                #pragma HLS UNROLL
                vals_buff.items[j] = val_block.items[j];
#if DEBUG 
                if (DEBUG&2) printf ("%.6f,", val_block.items[j]);
#endif
            }
#if DEBUG 
            if (DEBUG&2) printf ("\n");
#endif
            pkt_block v;
            vals_buff.set(v.data);
            out_values.write(v);
        }
    }
    
#if DEBUG 
    if (DEBUG&1) printf ("k1::read_values(): end\n");
#endif
}

void write_results(
        hls::stream<pkt_block>& res_stream,
        valb_t* vec_res,
        // prec_t* result,
        const unsigned int write_start_1,
        const unsigned int write_start_2,
        const unsigned int write_blocks,
        const unsigned int runs) {

    unsigned int write_start = write_start_1 + write_start_2;

#pragma HLS INLINE

#if DEBUG 
    if (DEBUG&1) printf ("k1::write_results(): start\n");
#endif
    res_write:
    assert(runs>0); 
    assert(write_blocks>0); // Helps inferring the compiler that the loop must be entered at least
    for (unsigned int i=0; i<write_blocks; i++) { // The possible trailing buffer is also wrote
        #pragma HLS PIPELINE II=1
        #pragma HLS loop_tripcount min=(vec_blk_min) max=(vec_blk_max)
        valvec_k2k_t res;
        pkt_block v = res_stream.read();
        res.get(v.data);
        valb_t res_buffer;
#if DEBUG 
        if (DEBUG&2) printf ("k1::result-block: %d\n", i);
#endif
        for (unsigned int j=0; j<BLOCK_SIZE; j++) {
            #pragma HLS UNROLL
            res_buffer.items[j] = res.items[j];
#if DEBUG 
            if (DEBUG&2) printf ("%.6f,", res.items[j]);
#endif
        }
#if DEBUG 
        if (DEBUG&2) printf ("\n");
#endif
        vec_res[write_start+i] = res_buffer;
    }

#if DEBUG 
    if (DEBUG&1) printf ("k1::write_results(): end\n");
#endif
}

extern "C" {

    void csr_spmv_repl_1(
            hls::stream<pkt_block>& out_indices,
            hls::stream<pkt_block>& out_values,
            hls::stream<pkt_block>& in_y,
            valb_t* values, 
            const intb_t* indices,
            // /*prec_t*/ valb_t* result,
            const unsigned int x_blocks_tot,
            const unsigned int row_blocks_tot, 
            const unsigned int y_blocks, 
            const unsigned int nnz_blocks_tot,
            const unsigned int runs) {

        #pragma HLS INTERFACE m_axi port=values offset=slave bundle=gmem0 max_read_burst_length=16 max_write_burst_length=16
        #pragma HLS INTERFACE m_axi port=indices offset=slave bundle=gmem1 max_read_burst_length=16 
        // // #pragma HLS INTERFACE m_axi port=result offset=slave bundle=gmem2 max_write_burst_length=16

        #pragma HLS INTERFACE axis port = out_indices
        #pragma HLS INTERFACE axis port = out_values
        #pragma HLS INTERFACE axis port = in_y

        #pragma HLS INTERFACE s_axilite port = x_blocks_tot
        #pragma HLS INTERFACE s_axilite port = row_blocks_tot
        #pragma HLS INTERFACE s_axilite port = y_blocks
        #pragma HLS INTERFACE s_axilite port = nnz_blocks_tot
        #pragma HLS INTERFACE s_axilite port = runs

        // #pragma HLS INTERFACE s_axilite port = result

#if DEBUG 
        if (DEBUG&1) printf ("k1::x_blocks_tot: %d\n", x_blocks_tot);
        if (DEBUG&1) printf ("k1::row_blocks_tot: %d\n", row_blocks_tot);
        if (DEBUG&1) printf ("k1::y_blocks: %d\n", y_blocks);
        if (DEBUG&1) printf ("k1::nnz_blocks_tot: %d\n", nnz_blocks_tot);
#endif
        #pragma HLS DATAFLOW

        read_indices(out_indices, indices, row_blocks_tot, nnz_blocks_tot, runs); 
        read_values(out_values, values, x_blocks_tot, nnz_blocks_tot, runs);
        write_results(in_y, values /*result*/, x_blocks_tot, nnz_blocks_tot, y_blocks, runs);
    }
}