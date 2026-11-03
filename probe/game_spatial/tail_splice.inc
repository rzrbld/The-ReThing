/* Insert immediately after L_0070C2B1, retaining original seed and final tail.
 * game_spatial_generation_valid must describe this exact container generation.
 * Until lifecycle writer closure, this MUST be false in live gameplay.
 * No guest calls, register macros, flags, FP stack, or memory writes in search.
 */
    if (game_spatial_generation_valid && ebp > 0 && ebp < 0x80000000u
            && ecx < ebp) {
        uint32_t spatial_t_bits = MEM32(esp + 0x10);
        if ((spatial_t_bits & 0x7f800000u) != 0x7f800000u) {
            float spatial_t;
            memcpy(&spatial_t, &spatial_t_bits, sizeof spatial_t);
            uint32_t spatial_lo=ecx, spatial_hi=ebp;
            while (spatial_lo<spatial_hi) {
                uint32_t spatial_mid=spatial_lo+(spatial_hi-spatial_lo)/2;
                uint32_t spatial_node=MEM32(esi+spatial_mid*4);
                uint32_t spatial_key_bits=MEM32(spatial_node+0x14);
                float spatial_key;
                memcpy(&spatial_key,&spatial_key_bits,sizeof spatial_key);
                if (spatial_key<=spatial_t) spatial_lo=spatial_mid+1;
                else spatial_hi=spatial_mid;
            }
            /* The unchanged original final iteration reconstructs guest state. */
            ecx = spatial_lo < ebp ? spatial_lo : ebp-1;
        }
    }
