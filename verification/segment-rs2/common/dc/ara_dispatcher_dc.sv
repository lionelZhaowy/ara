// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
// Original author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>.
// Test-only extraction and wrapper adaptation prepared with AI assistance.
// Ara source pin: 34bd3bc152421b4601a7bf3d6e8e91ffb545c99e.
module ara_dispatcher_dc import ara_dc_types_pkg::*; import ara_pkg::*; (
    // Clock and reset
    input  logic                                 clk_i,
    input  logic                                 rst_ni,
    // Interfaces with Ariane
    input  accelerator_req_t                     acc_req_i,
    output accelerator_resp_t                    acc_resp_o,
    // Interface with Ara's backend
    output ara_req_t                             ara_req_o,
    output logic                                 ara_req_valid_o,
    input  logic                                 ara_req_ready_i,
    input  ara_resp_t                            ara_resp_i,
    input  logic                                 ara_resp_valid_i,
    input  logic                                 ara_idle_i,
    // Interface with the lanes
    input  logic              [NrLanes-1:0][4:0] fflags_ex_i,
    input  logic              [NrLanes-1:0]      fflags_ex_valid_i,
    // LSU exception-related flush support
    output logic                                 lsu_ex_flush_o,
    input  logic                                 lsu_ex_flush_done_i,
    // Rounding mode is shared between all lanes
    input  logic              [NrLanes-1:0]      vxsat_flag_i,
    output vxrm_t             [NrLanes-1:0]      alu_vxrm_o,
    // Interface with the Vector Store Unit
    output logic                                 core_st_pending_o,
    input  logic                                 load_complete_i,
    input  logic                                 store_complete_i,
    input  logic                                 store_pending_i
);
  ara_dispatcher #(
    .NrLanes(2), .VLEN(2048), .CVA6Cfg(CVA6Cfg),
    .ara_req_t(ara_req_t), .ara_resp_t(ara_resp_t),
    .accelerator_req_t(accelerator_req_t),
    .accelerator_resp_t(accelerator_resp_t),
    .FPUSupport(FPUSupportHalfSingleDouble),
    .FPExtSupport(FPExtSupportEnable),
    .FixPtSupport(FixedPointEnable), .SegSupport(SegSupportEnable)
  ) i_dispatcher (
    .clk_i(clk_i), .rst_ni(rst_ni), .acc_req_i(acc_req_i), .acc_resp_o(acc_resp_o),
    .ara_req_o(ara_req_o), .ara_req_valid_o(ara_req_valid_o),
    .ara_req_ready_i(ara_req_ready_i), .ara_resp_i(ara_resp_i),
    .ara_resp_valid_i(ara_resp_valid_i), .ara_idle_i(ara_idle_i),
    .fflags_ex_i(fflags_ex_i), .fflags_ex_valid_i(fflags_ex_valid_i),
    .lsu_ex_flush_o(lsu_ex_flush_o), .lsu_ex_flush_done_i(lsu_ex_flush_done_i),
    .vxsat_flag_i(vxsat_flag_i), .alu_vxrm_o(alu_vxrm_o),
    .core_st_pending_o(core_st_pending_o), .load_complete_i(load_complete_i),
    .store_complete_i(store_complete_i), .store_pending_i(store_pending_i)
  );
endmodule
