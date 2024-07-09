#include <xlx_definitions.hpp>

/*
    (Multi-tile) CSR SpMV Logic split into four kernels.
        - Reader/Writer
        - Multiplier
        - Partial summer 
        - Accumulator
*/

void read_rows(
        hls::stream<pkt_ind_nnz> &out_row_tupple,
        hls::stream<indvec_k2k_t> &in_rows_str,
        const unsigned int y_len,
        const unsigned int row_blocks,
        const unsigned int tiles,
        const unsigned int runs) {
#if DEBUG 
    if (DEBUG&1) printf("k2::read_rows(): start\n");
#endif
    // XRT 2.15 i.e 2023.1: Pragma conflict happens on 'INLINE' and DATAFLOW pragmas: Inline into dataflow region may break the canonical form.
    // #pragma HLS INLINE

    int row_ptr[VECTOR_SIZE+BLOCK_SIZE];
    #pragma HLS ARRAY_PARTITION variable=row_ptr type=cyclic factor=8

    assert(runs>0);
    runs:
    for (unsigned int h=0; h<runs; h++) {
        #pragma HLS PIPELINE OFF

        assert(tiles>0);
        tiles:
        for (unsigned int tile=0; tile<tiles; tile++) {
            #pragma HLS PIPELINE OFF
            #pragma HLS LOOP_FLATTEN OFF
            #pragma HLS loop_tripcount min=(tiles_min) max=(tiles_max)
#if DEBUG
            if (DEBUG&1) printf ("k2::read_rows(): start of tile: %d\n", tile);
#endif
            // int row_blocks = row_blocks_vec[tile];
            // int y_len = y_len_vec[tile];

            indvec_k2k_t old_row_buffer; // 0 init

            row_read:
            for (unsigned int i=0; i<row_blocks; i++) { // The possible trailing buffer is also read
                #pragma HLS PIPELINE II=1
                #pragma HLS loop_tripcount min=(rows_blk_min) max=(rows_blk_max)

                indvec_k2k_t row_buffer = in_rows_str.read();
                #pragma HLS array_partition variable=row_buffer.items complete dim=0
#if DEBUG 
                if (DEBUG&2) printf ("k2::read_rows(): block read, id: %d\n", i);
#endif
                for (unsigned int j=0; j<BLOCK_SIZE; j++) {
                    #pragma HLS UNROLL
                    row_ptr[i*BLOCK_SIZE+j] = row_buffer.items[j]; 
#if DEBUG 
                    if (DEBUG&2) printf (", %d\n", row_buffer.items[j]);
#endif
                }
#if DEBUG
                if (DEBUG&2) printf ("\n");
#endif
            }

            row_write: 
            for (unsigned int i=0; i<=y_len; i++) {  
                #pragma HLS PIPELINE II=1
                #pragma HLS loop_tripcount min=(rows_min) max=(rows_max)

                indind_t row_item;
                row_item.index = i == y_len ? VECTOR_SIZE+BLOCK_SIZE-1 : i; // Invalid index...
                row_item.is_last = i == y_len;
                row_item.value = i == y_len ? BLOCK_SIZE : row_ptr[i+1] - row_ptr[i]; // Invalid size for "aggregation" single iteration
                
                pkt_ind_nnz v;
                row_item.set(v.data);

                if (row_item.value > 0) {
                    out_row_tupple.write(v); 
                }

#if DEBUG 
                if (DEBUG&2) printf ("k2::read_rows(): row sent:%d, i: %d", row_item.value > 0, i);
                if (DEBUG&2) printf (", nnzs: %d", row_item.value);
                if (DEBUG&2) printf (", is_last: %d\n", row_item.is_last);
#endif
            }
#if DEBUG
            if (DEBUG&1) printf ("k2::read_rows(): end of tile: %d\n", tile);
#endif
        }
    }
#if DEBUG 
    if (DEBUG&1) printf ("k2::read_rows(): end\n");
#endif
}

void read_products(
        hls::stream<pkt_block> &out_prod,
        hls::stream<valvec_k2k_t> &in_prod_str,
        const unsigned int nnz_blocks,
        const unsigned int runs) {
#if DEBUG 
    if (DEBUG&1) printf ("k2::read_products(): start\n");
#endif
    // XRT 2.15 i.e 2023.1: Pragma conflict happens on 'INLINE' and DATAFLOW pragmas: Inline into dataflow region may break the canonical form.
    // #pragma HLS INLINE

    assert(nnz_blocks>0);
    out_prods: 
    for (unsigned int h=0; h<runs; h++) {
        #pragma HLS PIPELINE OFF

        for (unsigned int i=0; i<nnz_blocks; i++) {
            #pragma HLS PIPELINE II=1
            #pragma HLS loop_tripcount min=(nnz_blk_min) max=(nnz_blk_max)

            valvec_k2k_t res_block = in_prod_str.read();
#if DEBUG 
            // if (DEBUG&2) printf ("k2::product-block: %d\n", i);
#endif
            pkt_block v;
            res_block.set(v.data);
            out_prod.write(v);
        }
    }
#if DEBUG 
        if (DEBUG&1) printf ("k2::read_products(): end\n");
#endif
}

// Writing to the output indirectly
void mult_values(
        hls::stream<indvec_k2k_t> &out_rows,
        hls::stream<valvec_k2k_t> &out_prod,
        hls::stream<pkt_block> &in_indices, 
        hls::stream<pkt_block> &in_values,
        const unsigned int x_blocks,
        const unsigned int row_blocks, 
        // const unsigned int nnz_blocks_vec[BLOCK_SIZE],
        const unsigned int tiles,
        const unsigned int runs) { 

    // XRT 2.15 i.e 2023.1: Pragma conflict happens on 'INLINE' and DATAFLOW pragmas: Inline into dataflow region may break the canonical form.
    // #pragma HLS INLINE
#if DEBUG 
    if (DEBUG&1) printf ("k2::mult_values(): start\n");
#endif
#define TILE_BLOCKS 5

    assert(runs>0);
    for (unsigned int h=0; h<runs; h++) {
        #pragma HLS PIPELINE OFF
        indvec_k2k_t nnz_blocks_vec;
        auto nnz_blocks_val = in_indices.read();
        nnz_blocks_vec.get(nnz_blocks_val.data);

        assert(tiles>0);
    tiles:
        for (unsigned int tile=0; tile<tiles; tile++) {
            #pragma HLS PIPELINE OFF
            #pragma HLS loop_flatten off
            #pragma HLS loop_tripcount min=(tiles_min) max=(tiles_max)
#if DEBUG
            if (DEBUG&1) printf ("k2::mult_values(): start of tile: %d\n", tile);
#endif
            int nnz_blocks = nnz_blocks_vec.items[tile];
#if DEBUG 
            if (DEBUG&1) printf ("k2::nnz_blocks: %d at current tile\n", nnz_blocks);
#endif
            out_rows: 
            for (unsigned int i=0; i<row_blocks; i++) {
                #pragma HLS PIPELINE II=1
                #pragma HLS loop_tripcount min=(rows_blk_min) max=(rows_blk_max)
                auto v = in_indices.read();
                indvec_k2k_t row_buffer;
                #pragma HLS array_partition variable=row_buffer.items complete dim=0
                row_buffer.get(v.data);
                out_rows.write(row_buffer);
            }

            // Todo: Fix the vector_size according to the distribution
            prec_t vector[VECTOR_SIZE+BLOCK_SIZE]; 
            #pragma HLS BIND_STORAGE variable=vector type=RAM_1WNR impl=BRAM
            // #pragma HLS ARRAY_RESHAPE variable=vector type=cyclic factor=16 
            #pragma HLS ARRAY_PARTITION variable=vector type=cyclic factor=4
            vec_read: 
            for (unsigned int i=0; i<x_blocks; i++) {
                #pragma HLS PIPELINE II=4
                #pragma HLS loop_tripcount min=(vec_blk_min) max=(vec_blk_max)
                pkt_block v = in_values.read();
                valvec_k2k_t vec_buffer;
                vec_buffer.get(v.data);
                for (unsigned int j=0; j<BLOCK_SIZE; j++) {
                    #pragma HLS UNROLL
                    vector[i*BLOCK_SIZE+j] = vec_buffer.items[j]; 
                }
            }

            // unsigned int nnzs_start = x_blocks;
            // unsigned int nnzs_end = x_blocks+nnz_blocks;
            values_mult_blocked: 
            for (int i=0; i<nnz_blocks; i++) {
                #pragma HLS PIPELINE II=1
                #pragma HLS loop_tripcount min=(nnz_blk_min) max=(nnz_blk_max)

                auto v1 = in_indices.read();
                auto v2 = in_values.read();
                
                indvec_k2k_t buff_cols;
                buff_cols.get(v1.data);
                valvec_k2k_t buff_values;
                buff_values.get(v2.data);
#if DEBUG 
                if (DEBUG&2) printf ("k2::mult-block: %d\n", i);
#endif
                valvec_k2k_t res_block;
                mult: 
                for (unsigned int j=0; j<BLOCK_SIZE; j++) {
                    #pragma HLS UNROLL
                    // #pragma HLS BIND_OP variable=res_block.items op=fmul impl=meddsp
                    res_block.items[j] = buff_values.items[j] * vector[buff_cols.items[j]];
#if DEBUG 
                    if (DEBUG&2) printf ("%f*%f,", res_block.items[j], vector[buff_cols.items[j]]);
#endif
                }
#if DEBUG 
                if (DEBUG&2) printf ("\n");
#endif
                out_prod.write(res_block);
            }
   
#if DEBUG
        if (DEBUG&1) printf ("k2::mult_values(): end of tile: %d\n", tile);
#endif
        } 
                  
    }

#if DEBUG 
        if (DEBUG&1) printf ("k2::mult_values(): end\n");
#endif
}

extern "C" {

    void csr_spmv_repl_2(
            hls::stream<pkt_ind_nnz>& out_row_tupples,
            hls::stream<pkt_block>& out_prod,
            hls::stream<pkt_block>& in_indices,
            hls::stream<pkt_block>& in_values,
            const unsigned int x_blocks,
            const unsigned int row_blocks,
            const unsigned int y_len, 
            // const unsigned int nnz_blocks_vec[BLOCK_SIZE],
            const unsigned int tiles,
            const unsigned int nnz_blocks_tot,
            const unsigned int runs) {
        
        #pragma HLS INTERFACE axis port = out_row_tupples
        #pragma HLS INTERFACE axis port = out_prod
        #pragma HLS INTERFACE axis port = in_indices
        #pragma HLS INTERFACE axis port = in_values

        // #pragma HLS INTERFACE m_axi port = nnz_blocks_vec offset=slave max_read_burst_length=16 // bundle=gmem2

        #pragma HLS INTERFACE s_axilite port = x_blocks 
        #pragma HLS INTERFACE s_axilite port = row_blocks
        #pragma HLS INTERFACE s_axilite port = y_len
        #pragma HLS INTERFACE s_axilite port = tiles
        #pragma HLS INTERFACE s_axilite port = nnz_blocks_tot

#if DEBUG 
        if (DEBUG&1) printf ("k2::x_blocks: %d\n", x_blocks);
        if (DEBUG&1) printf ("k2::row_blocks: %d\n", row_blocks);
        if (DEBUG&1) printf ("k2::y_len: %d\n", y_len);
        if (DEBUG&1) printf ("k2::tiles: %d\n", tiles);
        if (DEBUG&1) printf ("k2::nnz_blocks_tot: %d\n", nnz_blocks_tot);
#endif

        const unsigned int str_depth = 2048; //
        static hls::stream<indvec_k2k_t> rows_stream;
        static hls::stream<valvec_k2k_t> prod_stream;
        #pragma HLS STREAM variable=rows_stream depth=str_depth 
        #pragma HLS STREAM variable=prod_stream depth=16 

        #pragma HLS DATAFLOW

        // mult_values(rows_stream, out_prod, in_indices, in_values, x_blocks, row_blocks, nnz_blocks_vec, tiles);
        // read_rows(out_row_tupples, rows_stream, y_len, row_blocks, tiles);

        mult_values(rows_stream, prod_stream, in_indices, in_values, x_blocks, row_blocks, /*nnz_blocks_vec,*/ tiles, runs);
        read_products(out_prod, prod_stream, nnz_blocks_tot, runs);
        read_rows(out_row_tupples, rows_stream, y_len, row_blocks, tiles, runs); 
    }
}