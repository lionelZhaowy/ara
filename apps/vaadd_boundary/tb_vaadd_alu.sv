// SPDX-License-Identifier: Apache-2.0
// Ara local reproducer. Compile with the frozen real ara_pkg, rvv_pkg and
// either baseline or candidate simd_alu; do not compile two simd_alu modules.
module tb_ara_test_vaadd_alu;
  timeunit 1ns;
  timeprecision 1ps;

  import ara_pkg::*;
  import rvv_pkg::*;

  localparam int unsigned ExpectedCases = 256;
  localparam int unsigned MaxErrorLines = 8;

  elen_t operand_a, operand_b, result;
  logic valid, vm, narrowing_select;
  logic [7:0] mask, scale_round;
  ara_op_e op;
  vew_e vew;
  vxrm_t vxrm;
  vxsat_t vxsat;

  simd_alu #(.FixPtSupport(FixedPointEnable)) dut (
    .operand_a_i(operand_a), .operand_b_i(operand_b),
    .valid_i(valid), .vm_i(vm), .mask_i(mask),
    .narrowing_select_i(narrowing_select), .op_i(op), .vew_i(vew),
    .vxsat_o(vxsat), .rm(scale_round), .vxrm_i(vxrm), .result_o(result)
  );

  initial begin : deadline
    #1us; $fatal(1, "VAADD unit HDL deadline");
  end

  initial begin : run_vectors
    string path, line, trailing;
    int fd, parsed, op_selector, sew, rounding_mode;
    int unsigned cases, checks, errors, line_number;
    bit wrong_gold;
    elen_t expected;

    path = "vectors.txt";
    if ($value$plusargs("ARA_TEST_VAADD_VECTORS=%s", path)) begin end
    wrong_gold = $test$plusargs("ARA_TEST_WRONG_GOLD");
    cases = 0;
    checks = 0;
    errors = 0;
    line_number = 0;
    operand_a = '0;
    operand_b = '0;
    valid = 1'b0;
    vm = 1'b1;
    mask = '1;
    narrowing_select = 1'b0;
    scale_round = '0; // VAADD use vxrm_i, not this scaling-round input.
    op = VAADD;
    vew = EW8;
    vxrm = '0;
    if ($bits(elen_t) != 64 || $bits(vxsat_t) != 8)
      $fatal(1, "ARA_TEST_VAADD_ALU_INPUT_ERROR unexpected real package widths");
    fd = $fopen(path, "r");
    if (fd == 0)
      $fatal(1, "ARA_TEST_VAADD_ALU_INPUT_ERROR cannot open %s", path);
    $display("ARA_TEST_VAADD_ALU_START expected_cases=%0d wrong_gold=%0d vectors=%s",
             ExpectedCases, wrong_gold, path);

    // Each line: signed(1=VAADD), SEW, vxrm, A64, B64, expected64.
    // The real ALU averages signed B+A. No arithmetic oracle is duplicated in SV.
    while ($fgets(line, fd) != 0) begin
      line_number++;
      parsed = $sscanf(line, "%d %d %d %h %h %h %s", op_selector, sew,
                       rounding_mode, operand_a, operand_b, expected, trailing);
      if (parsed != 6 || op_selector != 1 ||
          !(sew inside {8, 16, 32, 64}) || rounding_mode < 0 || rounding_mode > 3 ||
          $isunknown({operand_a, operand_b, expected}))
        $fatal(1, "ARA_TEST_VAADD_ALU_INPUT_ERROR malformed line=%0d parsed=%0d",
               line_number, parsed);
      op = VAADD;
      case (sew)
        8:  vew = EW8;
        16: vew = EW16;
        32: vew = EW32;
        64: vew = EW64;
      endcase
      vxrm = vxrm_t'(rounding_mode);
      valid = 1'b1;
      if (wrong_gold && cases == 0) expected[0] = ~expected[0];
      #1ns;
      cases++;
      checks++;
      if (result !== expected) begin
        errors++;
        if (errors <= MaxErrorLines)
          $display("ARA_TEST_VAADD_ALU_ERROR case=%0d signed=%0d sew=%0d vxrm=%0d A=%016h B=%016h result=%016h expected=%016h",
                   cases, op_selector, sew, rounding_mode, operand_a, operand_b,
                   result, expected);
      end
      checks++;
      if (vxsat !== 8'h00) begin
        errors++;
        if (errors <= MaxErrorLines)
          $display("ARA_TEST_VAADD_ALU_ERROR case=%0d vxsat=%02h expected=00", cases, vxsat);
      end
    end
    $fclose(fd);
    valid = 1'b0;
    checks++;
    if (cases != ExpectedCases) begin
      errors++;
      if (errors <= MaxErrorLines)
        $display("ARA_TEST_VAADD_ALU_ERROR cases=%0d expected_cases=%0d", cases, ExpectedCases);
    end
    $display("ARA_TEST_VAADD_ALU_DONE cases=%0d checks=%0d errors=%0d wrong_gold=%0d",
             cases, checks, errors, wrong_gold);
    if (errors != 0) $fatal(1, "ARA_TEST_VAADD_ALU_FAIL");
    $finish;
  end
endmodule
