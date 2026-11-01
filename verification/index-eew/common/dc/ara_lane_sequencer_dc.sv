// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
// Original author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>.
// Test-only extraction and wrapper adaptation prepared with AI assistance.
// Ara source pin: 34bd3bc152421b4601a7bf3d6e8e91ffb545c99e.
module ara_lane_sequencer_dc
  import ara_pkg::*; import rvv_pkg::*; import cf_math_pkg::idx_width; import ara_dc_types_pkg::*;
(
    input  logic                                          clk_i,
    input  logic                                          rst_ni,
    // Lane ID
    input  logic                 [idx_width(NrLanes)-1:0] lane_id_i,
    // Interface with the main sequencer
    input  pe_req_t                                       pe_req_i,
    input  logic                                          pe_req_valid_i,
    input  logic                 [NrVInsn-1:0]            pe_vinsn_running_i,
    output logic                                          pe_req_ready_o,
    output pe_resp_t                                      pe_resp_o,
    // Support for store exception flush
    input  logic                                          lsu_ex_flush_i,
    output logic                                          lsu_ex_flush_o,
    // Interface with the operand requester
    output operand_request_cmd_t [NrOperandQueues-1:0]    operand_request_o,
    output logic                 [NrOperandQueues-1:0]    operand_request_valid_o,
    input  logic                 [NrOperandQueues-1:0]    operand_request_ready_i,
    output logic                                          alu_vinsn_done_o,
    output logic                                          mfpu_vinsn_done_o,
    // Interface with the Operand Queue (MaskB - for VRGATHER)
    input  logic                                          mask_b_cmd_pop_i,
    // Interface with the lane's VFUs
    output vfu_operation_t                                vfu_operation_o,
    output logic                                          vfu_operation_valid_o,
    input  logic                                          alu_ready_i,
    input  logic                 [NrVInsn-1:0]            alu_vinsn_done_i,
    input  logic                                          mfpu_ready_i,
    input  logic                 [NrVInsn-1:0]            mfpu_vinsn_done_i,
    // Masku interface for vrgather/vcompress
    input  logic                                          masku_vrgat_req_valid_i,
    output logic                                          masku_vrgat_req_ready_o,
    input  vrgat_req_t                                    masku_vrgat_req_i
  );
  lane_sequencer #(
    .NrLanes(2), .pe_req_t(pe_req_t), .pe_resp_t(pe_resp_t),
    .operand_request_cmd_t(operand_request_cmd_t), .vfu_operation_t(vfu_operation_t)
  ) i_dut (.*);
endmodule
