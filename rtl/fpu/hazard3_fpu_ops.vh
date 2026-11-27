// Internal FPU operation and rounding-mode encodings (include inside a module).
// SPDX-License-Identifier: Apache-2.0

localparam [4:0] FPU_ADD      = 5'd0;
localparam [4:0] FPU_SUB      = 5'd1;
localparam [4:0] FPU_MUL      = 5'd2;
localparam [4:0] FPU_MADD     = 5'd3;   //  (a * b) + c
localparam [4:0] FPU_MSUB     = 5'd4;   //  (a * b) - c
localparam [4:0] FPU_NMSUB    = 5'd5;   // -(a * b) + c
localparam [4:0] FPU_NMADD    = 5'd6;   // -(a * b) - c
localparam [4:0] FPU_DIV      = 5'd7;
localparam [4:0] FPU_SQRT     = 5'd8;
localparam [4:0] FPU_SGNJ     = 5'd9;
localparam [4:0] FPU_SGNJN    = 5'd10;
localparam [4:0] FPU_SGNJX    = 5'd11;
localparam [4:0] FPU_MIN      = 5'd12;
localparam [4:0] FPU_MAX      = 5'd13;
localparam [4:0] FPU_EQ       = 5'd14;
localparam [4:0] FPU_LT       = 5'd15;
localparam [4:0] FPU_LE       = 5'd16;
localparam [4:0] FPU_CLASS    = 5'd17;
localparam [4:0] FPU_CVT_W_S  = 5'd18;
localparam [4:0] FPU_CVT_WU_S = 5'd19;
localparam [4:0] FPU_CVT_S_W  = 5'd20;
localparam [4:0] FPU_CVT_S_WU = 5'd21;

// Rounding modes, same encoding as the RISC-V rm field (DYN resolved by the core)
localparam [2:0] RM_RNE = 3'd0;
localparam [2:0] RM_RTZ = 3'd1;
localparam [2:0] RM_RDN = 3'd2;
localparam [2:0] RM_RUP = 3'd3;
localparam [2:0] RM_RMM = 3'd4;

// fflags bit positions: {NV, DZ, OF, UF, NX}
localparam FLAG_NV = 4;
localparam FLAG_DZ = 3;
localparam FLAG_OF = 2;
localparam FLAG_UF = 1;
localparam FLAG_NX = 0;

localparam [31:0] F32_CANONICAL_NAN = 32'h7fc00000;
