// SPDX-License-Identifier: Apache-2.0
// Public test support prepared with AI assistance. No DUT logic is copied here.
// Interface: Ara 34bd3bc152421b4601a7bf3d6e8e91ffb545c99e,
// CVA6 99eac9a649001bdf5b8f9da52e0ca73d5c48db1c,
// tech_cells_generic 7968dd6e6180df2c644636bc6d2908a49f2190cf.
// Add this module as a second top. It never drives or stops the DUT.
module ara_observer;
  timeunit 1ns;
  timeprecision 1ps;

  `define V01_CPU ara_tb.dut.i_ara_soc.i_system.i_ariane
  `define V01_SOC ara_tb.dut.i_ara_soc
  localparam int MaxRegions = 8;
  localparam logic [63:0] DramBase = 64'h8000_0000;
  bit enabled, initialized;
  int fd, regions, fence_count, exit_count, late_writes, consistency_errors;
  logic [63:0] fence_pc, base[MaxRegions];
  int unsigned words[MaxRegions];
  string names[MaxRegions];

  initial begin
    enabled = $test$plusargs("V01_OBSERVER");
    if (enabled) begin
      fd = $fopen("v01_observer.log", "w");
      if (!fd) $fatal(1, "Cannot open v01_observer.log");
      if (!$value$plusargs("V01_FENCE_PC=%h", fence_pc))
        $fatal(1, "V01_OBSERVER requires V01_FENCE_PC");
      if (!$value$plusargs("V01_REGIONS=%d", regions) || regions < 1 || regions > MaxRegions)
        $fatal(1, "V01_REGIONS must be 1..8");
      if (ara_tb.NrLanes != 2 || ara_tb.VLEN != 2048 ||
          `V01_CPU.CVA6Cfg.DCacheType != config_pkg::WT ||
          `V01_CPU.CVA6Cfg.NrPMPEntries != 0)
        $fatal(1, "Observer requires the fixed two-lane WT Ara configuration");
      $fdisplay(fd, "V01_CFG lanes=%0d vlen=%0d wt=1 pmp=%0d dram_base=%016h dram_bytes=%0d fence_pc=%016h regions=%0d",
                ara_tb.NrLanes, ara_tb.VLEN, `V01_CPU.CVA6Cfg.NrPMPEntries,
                DramBase, $size(`V01_SOC.i_dram.sram) * 8, fence_pc, regions);
      for (int i = 0; i < regions; i++) begin
        if (!$value$plusargs($sformatf("V01_R%0d_BASE=%%h", i), base[i]) ||
            !$value$plusargs($sformatf("V01_R%0d_WORDS=%%d", i), words[i]) ||
            !$value$plusargs($sformatf("V01_R%0d_NAME=%%s", i), names[i]))
          $fatal(1, "Region %0d requires BASE, WORDS and ELF symbol NAME", i);
        if (words[i] == 0 || base[i][1:0] != 0 || base[i] < DramBase ||
            base[i] - DramBase >= $size(`V01_SOC.i_dram.sram) * 64'd8 ||
            words[i] > ($size(`V01_SOC.i_dram.sram) * 64'd8 - (base[i] - DramBase)) / 4)
          $fatal(1, "Region %0d exceeds the actual SRAM or is not word aligned", i);
        $fdisplay(fd, "V01_REGION index=%0d name=%s base=%016h words=%0d file=v01_region_%0d.hex",
                  i, names[i], base[i], words[i], i);
      end
      initialized = 1;
    end
  end

  // Sample the same pre-edge values that the commit stage uses.
  always @(posedge ara_tb.clk) begin
    if (enabled && initialized && ara_tb.rst_n === 1'b1) begin
      if (`V01_CPU.rst_uarch_n === 1'b1 && `V01_SOC.i_system.acc_cons_en !== 1'b1)
        consistency_errors++;
      if (`V01_CPU.commit_instr_id_commit[0].valid === 1'b1 &&
          `V01_CPU.commit_ack_commit_id[0] === 1'b1 &&
          `V01_CPU.commit_drop_id_commit[0] === 1'b0 &&
          `V01_CPU.commit_instr_id_commit[0].ex.valid === 1'b0 &&
          `V01_CPU.fence_commit_controller === 1'b1 &&
          `V01_CPU.commit_instr_id_commit[0].pc == fence_pc) begin
        fence_count++;
        $fdisplay(fd, "V01_FENCE time_ns=%0.3f pc=%016h valid=%b ack=%b lsu_empty=%b wt_empty=%b all_empty=%b acc_cons=%b",
                  $realtime, `V01_CPU.commit_instr_id_commit[0].pc,
                  `V01_CPU.commit_instr_id_commit[0].valid,
                  `V01_CPU.commit_ack_commit_id[0], `V01_CPU.no_st_pending_ex,
                  `V01_CPU.dcache_commit_wbuffer_empty, `V01_CPU.no_st_pending_commit,
                  `V01_SOC.i_system.acc_cons_en);
      end
      if ($test$plusargs("V01_VECTOR_COMMIT"))
        for (int c = 0; c < `V01_CPU.CVA6Cfg.NrCommitPorts; c++)
          if (`V01_CPU.commit_instr_id_commit[c].valid === 1'b1 &&
              `V01_CPU.commit_ack_commit_id[c] === 1'b1 &&
              `V01_CPU.commit_drop_id_commit[c] === 1'b0 &&
              `V01_CPU.commit_instr_id_commit[c].ex.valid === 1'b0 &&
              `V01_CPU.commit_instr_id_commit[c].fu == ariane_pkg::ACCEL)
            $fdisplay(fd, "V01_VECTOR_COMMIT time_ns=%0.3f pc=%016h op=%0d port=%0d",
                      $realtime, `V01_CPU.commit_instr_id_commit[c].pc,
                      `V01_CPU.commit_instr_id_commit[c].op, c);
      // A late write invalidates the fence witness for that result region.
      if (fence_count != 0 && `V01_SOC.l2_req === 1'b1 && `V01_SOC.l2_we === 1'b1)
        for (int b = 0; b < 8; b++)
          if (`V01_SOC.l2_be[b] === 1'b1)
            for (int r = 0; r < regions; r++)
              if ((`V01_SOC.l2_addr & ~64'd7) + b >= base[r] &&
                  (`V01_SOC.l2_addr & ~64'd7) + b < base[r] + 64'd4 * words[r]) begin
                late_writes++;
                $fdisplay(fd, "V01_LATE_WRITE time_ns=%0.3f region=%0d address=%016h", $realtime, r,
                          (`V01_SOC.l2_addr & ~64'd7) + b);
              end
    end
  end

  // The native TB owns completion. A final block also runs on an HDL cutoff;
  // exit_count distinguishes that partial diagnostic from a normal exit.
  always @(posedge ara_tb.exit[0]) begin
    if (enabled && initialized) begin
      exit_count++;
      $fdisplay(fd, "V01_EXIT time_ns=%0.3f raw=%016h target=%0d", $realtime,
                ara_tb.exit, ara_tb.exit >> 1);
    end
  end

  final begin
    if (enabled && initialized) begin
      for (int r = 0; r < regions; r++) begin
        int out_fd;
        logic [63:0] address, row;
        logic [31:0] word_value;
        out_fd = $fopen($sformatf("v01_region_%0d.hex", r), "w");
        if (out_fd) begin
          for (int unsigned w = 0; w < words[r]; w++) begin
            address = base[r] + 64'd4 * w;
            row = (address - DramBase) >> 3;
            word_value = `V01_SOC.i_dram.sram[row][(address[2] ? 32 : 0) +: 32];
            $fdisplay(out_fd, "%08h", word_value);
          end
          $fclose(out_fd);
        end else $fdisplay(fd, "V01_DUMP_ERROR region=%0d", r);
      end
      $fdisplay(fd, "V01_FINAL time_ns=%0.3f fences=%0d exits=%0d late_writes=%0d consistency_errors=%0d raw_exit=%016h",
                $realtime, fence_count, exit_count, late_writes, consistency_errors, ara_tb.exit);
      $fclose(fd);
    end
  end
  `undef V01_CPU
  `undef V01_SOC
endmodule
