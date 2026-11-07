// SPDX-License-Identifier: Apache-2.0
`define ARA_OBS ara_tb.dut.i_ara_soc.i_system.i_ara
module extension_source_observer;
  import ara_pkg::*;
  int source_reshuffles, source_errors, layout_checks;
  bit layout_seen;
  always @(posedge `ARA_OBS.clk_i) begin
    if (!`ARA_OBS.rst_ni) begin
      source_reshuffles = 0; source_errors = 0; layout_checks = 0; layout_seen = 0;
    end
    else begin
      if (`ARA_OBS.i_dispatcher.is_decoding &&
          `ARA_OBS.i_dispatcher.is_integer_extension &&
          (`ARA_OBS.i_dispatcher.instr.instr[24:20] == 5'd16) &&
          (`ARA_OBS.i_dispatcher.state_q == 2'd0) && !layout_seen) begin
        layout_seen = 1; layout_checks++;
        $display("EXT_SOURCE_LAYOUT eew16=%0d eew17=%0d eew18=%0d eew19=%0d",
          `ARA_OBS.i_dispatcher.eew_q[16], `ARA_OBS.i_dispatcher.eew_q[17],
          `ARA_OBS.i_dispatcher.eew_q[18], `ARA_OBS.i_dispatcher.eew_q[19]);
        if (`ARA_OBS.i_dispatcher.eew_q[16] != 1 ||
            `ARA_OBS.i_dispatcher.eew_q[17] != 0 ||
            `ARA_OBS.i_dispatcher.eew_q[18] != 0 ||
            `ARA_OBS.i_dispatcher.eew_q[19] != 0) begin
          source_errors++; $error("EXT_OBS fixture did not create the mixed source EEW");
        end
      end
      if (`ARA_OBS.i_dispatcher.is_integer_extension &&
          (`ARA_OBS.i_dispatcher.instr.instr[19:15] inside {5'd6, 5'd7}) &&
          (`ARA_OBS.i_dispatcher.instr.instr[24:20] == 5'd16) &&
          (`ARA_OBS.i_dispatcher.state_q == 2'd3) &&
          `ARA_OBS.i_dispatcher.reshuffle_req_q[1] &&
          !`ARA_OBS.i_dispatcher.reshuffle_req_q[0]) begin
        if (`ARA_OBS.i_dispatcher.rs_lmul_cnt_limit_q != 3 ||
            `ARA_OBS.i_dispatcher.vs_buffer_q < 16 ||
            `ARA_OBS.i_dispatcher.vs_buffer_q >= 20) begin
          source_errors++; $error("EXT_OBS source scan exceeded four-register EMUL");
        end
      end
      if (`ARA_OBS.i_dispatcher.ara_req_valid_o &&
             `ARA_OBS.i_dispatcher.ara_req_ready_i &&
             `ARA_OBS.i_dispatcher.is_integer_extension &&
             (`ARA_OBS.i_dispatcher.instr.instr[19:15] inside {5'd6, 5'd7}) &&
             (`ARA_OBS.i_dispatcher.instr.instr[24:20] == 5'd16) &&
             (`ARA_OBS.i_dispatcher.ara_req_o.op == VSLIDEDOWN)) begin
      source_reshuffles++;
      $display("EXT_RESHUFFLE vd=%0d", `ARA_OBS.i_dispatcher.ara_req_o.vd);
      if (`ARA_OBS.i_dispatcher.ara_req_o.vd < 16 ||
          `ARA_OBS.i_dispatcher.ara_req_o.vd >= 20) begin
        source_errors++; $error("EXT_OBS reshape escaped source EMUL");
      end
      end
      if (`ARA_OBS.i_dispatcher.ara_req_valid_o &&
          `ARA_OBS.i_dispatcher.ara_req_ready_i &&
          (`ARA_OBS.i_dispatcher.ara_req_o.op == VADD) &&
          (`ARA_OBS.i_dispatcher.ara_req_o.vs2 == 16) &&
          (`ARA_OBS.i_dispatcher.ara_req_o.vd == 24)) layout_seen = 0;
    end
  end
  final begin
    $display("EXT_RESHUFFLE_DONE count=%0d layouts=%0d errors=%0d",
             source_reshuffles, layout_checks, source_errors);
    if (source_reshuffles != 6 || layout_checks != 2 || source_errors != 0)
      $error("EXT_OBS source reshuffle count mismatch");
  end
endmodule
`undef ARA_OBS
