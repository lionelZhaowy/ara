// SPDX-License-Identifier: Apache-2.0
// Public test support prepared with AI assistance. Add as a passive top.
module ara_mask_observer;
  timeunit 1ns; timeprecision 1ps;
// Passive reads only. No bind, DUT ports, drives, force or completion override.
// +ARA_TEST_MASK_START_OBSERVE enables all target events without truncation.
`define ARA_TEST_MASK_A ara_tb.dut.i_ara_soc.i_system.i_ara
`define ARA_TEST_MASK_M `ARA_TEST_MASK_A.i_masku
`define ARA_TEST_MASK_D `ARA_TEST_MASK_A.i_vlsu.i_vldu
`define ARA_TEST_MASK_S `ARA_TEST_MASK_A.i_vlsu.i_vstu
`define ARA_TEST_MASK_L `ARA_TEST_MASK_A.gen_lanes[ara_test_mask_lane].i_lane.i_lane_sequencer

bit ara_test_mask_enabled = 0;
longint unsigned ara_test_mask_cycle = 0;
longint unsigned ara_test_mask_accepted = 0, ara_test_mask_emitted = 0, ara_test_mask_done = 0;
longint unsigned ara_test_mask_snapshot_ns=0, ara_test_mask_snapshot_observed_ns=0;
event ara_test_mask_snapshot;
initial begin
  if ($test$plusargs("ARA_TEST_MASK_START_OBSERVE")) begin
    if (!$value$plusargs("ARA_TEST_MASK_SNAPSHOT_NS=%d",ara_test_mask_snapshot_ns) ||
        ara_test_mask_snapshot_ns==0)
      $fatal(1,"ARA_TEST passive mask snapshot needs the frozen positive HDL deadline");
    #(ara_test_mask_snapshot_ns * 1ns);
    // #0 moves behind the active-edge observers. Each $strobe below reads
    // the counters and DUT q-state in the postponed region, after this
    // timestamp's NBA updates. No HDL time advances and no DUT is driven.
    #0;
    ara_test_mask_snapshot_observed_ns=longint'($realtime / 1ns);
    ->ara_test_mask_snapshot;
  end
end
initial begin
  ara_test_mask_enabled = $test$plusargs("ARA_TEST_MASK_START_OBSERVE");
  if (ara_test_mask_enabled) begin
    if (`ARA_TEST_MASK_A.NrLanes != 2 || `ARA_TEST_MASK_A.VLEN != 2048 ||
        ara_pkg::MaskuInsnQueueDepth != 1 ||
        $bits(`ARA_TEST_MASK_M.masku_operand_m[0]) != 64 ||
        $bits(`ARA_TEST_MASK_M.pe_req_i.vl) != 12)
      $fatal(1,"ARA_TEST mask observer requires actual 2 lanes/VLEN2048/ELEN64/depth1/12-bit VL");
    $display("ARA_TEST_MASK_UNIT_READY lanes=2 depth=1 vlenbits=12");
  end
end

// Record disturbances even when no lane request is pending.
// The initial reset release is outside the firmware phase interval.
always @(ara_tb.rst_n) if ($test$plusargs("ARA_TEST_MASK_START_OBSERVE"))
  $display("ARA_TEST_MASK_RESET cycle=%0d time_ns=%0d level=%b",
    ara_test_mask_cycle,longint'($realtime / 1ns),ara_tb.rst_n);
always @(posedge ara_tb.clk) if (ara_tb.rst_n && ara_test_mask_enabled) begin
  if (`ARA_TEST_MASK_D.lsu_ex_flush_q || `ARA_TEST_MASK_S.lsu_ex_flush_q ||
      `ARA_TEST_MASK_A.gen_lanes[0].i_lane.i_lane_sequencer.lsu_ex_flush_o ||
      `ARA_TEST_MASK_A.gen_lanes[1].i_lane.i_lane_sequencer.lsu_ex_flush_o)
    $display("ARA_TEST_MASK_FLUSH cycle=%0d load=%b store=%b lane0=%b lane1=%b",
      ara_test_mask_cycle,`ARA_TEST_MASK_D.lsu_ex_flush_q,`ARA_TEST_MASK_S.lsu_ex_flush_q,
      `ARA_TEST_MASK_A.gen_lanes[0].i_lane.i_lane_sequencer.lsu_ex_flush_o,
      `ARA_TEST_MASK_A.gen_lanes[1].i_lane.i_lane_sequencer.lsu_ex_flush_o);
  if (!`ARA_TEST_MASK_M.vinsn_queue_full && `ARA_TEST_MASK_M.pe_req_valid_i &&
      !`ARA_TEST_MASK_M.vinsn_running_q[`ARA_TEST_MASK_M.pe_req_i.id] &&
      (!`ARA_TEST_MASK_M.pe_req_i.vm || `ARA_TEST_MASK_M.pe_req_i.vfu == ara_pkg::VFU_MaskUnit) &&
      !(`ARA_TEST_MASK_M.pe_req_i.vfu inside {ara_pkg::VFU_LoadUnit,ara_pkg::VFU_StoreUnit}))
    $display("ARA_TEST_MASK_OTHER_ACCEPT cycle=%0d id=%0d vfu=%0d op=%0d vm=%b",
      ara_test_mask_cycle,`ARA_TEST_MASK_M.pe_req_i.id,`ARA_TEST_MASK_M.pe_req_i.vfu,
      `ARA_TEST_MASK_M.pe_req_i.op,`ARA_TEST_MASK_M.pe_req_i.vm);
end

for (genvar ara_test_mask_lane=0; ara_test_mask_lane<2; ara_test_mask_lane++) begin: gen_ara_test_mask_lanes
  longint unsigned pushes=0, handshakes=0, stalls=0, hold_errors=0;
  bit pending=0, held=0;
  logic [255:0] held_payload;
  // Hazards are intentionally excluded: the real lane continuously clears
  // hazard bits while a request waits. Its identity/address/control stay fixed.
  wire [255:0] fixed_payload = 256'({
    `ARA_TEST_MASK_L.operand_request_o[ara_pkg::MaskM].id,
    `ARA_TEST_MASK_L.operand_request_o[ara_pkg::MaskM].vs,
    `ARA_TEST_MASK_L.operand_request_o[ara_pkg::MaskM].scale_vl,
    `ARA_TEST_MASK_L.operand_request_o[ara_pkg::MaskM].cvt_resize,
    `ARA_TEST_MASK_L.operand_request_o[ara_pkg::MaskM].is_reduct,
    `ARA_TEST_MASK_L.operand_request_o[ara_pkg::MaskM].is_slide,
    `ARA_TEST_MASK_L.operand_request_o[ara_pkg::MaskM].eew,
    `ARA_TEST_MASK_L.operand_request_o[ara_pkg::MaskM].conv,
    `ARA_TEST_MASK_L.operand_request_o[ara_pkg::MaskM].target_fu,
    `ARA_TEST_MASK_L.operand_request_o[ara_pkg::MaskM].vtype,
    `ARA_TEST_MASK_L.operand_request_o[ara_pkg::MaskM].vl,
    `ARA_TEST_MASK_L.operand_request_o[ara_pkg::MaskM].vstart});
  initial if ($test$plusargs("ARA_TEST_MASK_START_OBSERVE"))
    $display("ARA_TEST_MASK_LANE_READY lane=%0d lanes=2 reqbits=%0d vlenbits=%0d",
             ara_test_mask_lane, $bits(`ARA_TEST_MASK_L.operand_request_o[ara_pkg::MaskM]),
             $bits(`ARA_TEST_MASK_L.pe_req.vl));
  always @(posedge ara_tb.clk) begin
    if (!ara_tb.rst_n) begin pending=0; held=0; end
    else if (ara_test_mask_enabled) begin
      if (held && !`ARA_TEST_MASK_L.lsu_ex_flush_o &&
          (`ARA_TEST_MASK_L.operand_request_valid_o[ara_pkg::MaskM] !== 1'b1 ||
           fixed_payload !== held_payload)) begin
        hold_errors++;
        $display("ARA_TEST_MASK_LANE_HOLD_ERROR lane=%0d cycle=%0d",ara_test_mask_lane,ara_test_mask_cycle);
      end
      // Retire the old output before recording a same-cycle replacement.
      if (`ARA_TEST_MASK_L.operand_request_valid_o[ara_pkg::MaskM] &&
          `ARA_TEST_MASK_L.operand_request_ready_i[ara_pkg::MaskM] && pending) begin
        handshakes++;
        $display("ARA_TEST_MASK_LANE_HANDSHAKE lane=%0d cycle=%0d serial=%0d id=%0d start=%0d count=%0d eew=%0d vs=%0d scale=%0d",
          ara_test_mask_lane,ara_test_mask_cycle,pushes,
          `ARA_TEST_MASK_L.operand_request_o[ara_pkg::MaskM].id,
          `ARA_TEST_MASK_L.operand_request_o[ara_pkg::MaskM].vstart,
          `ARA_TEST_MASK_L.operand_request_o[ara_pkg::MaskM].vl,
          `ARA_TEST_MASK_L.operand_request_o[ara_pkg::MaskM].eew,
          `ARA_TEST_MASK_L.operand_request_o[ara_pkg::MaskM].vs,
          `ARA_TEST_MASK_L.operand_request_o[ara_pkg::MaskM].scale_vl);
        pending=0;
      end
      if (`ARA_TEST_MASK_L.lsu_ex_flush_o) begin
        if (pending) $display("ARA_TEST_MASK_LANE_FLUSH lane=%0d cycle=%0d serial=%0d",ara_test_mask_lane,ara_test_mask_cycle,pushes);
        pending=0;
      end
      if (`ARA_TEST_MASK_L.operand_request_push[ara_pkg::MaskM] &&
          !`ARA_TEST_MASK_L.pe_req.vm &&
          (`ARA_TEST_MASK_L.pe_req.vfu inside {ara_pkg::VFU_LoadUnit,ara_pkg::VFU_StoreUnit})) begin
        if (pending) $display("ARA_TEST_MASK_LANE_OVERWRITE lane=%0d cycle=%0d serial=%0d",ara_test_mask_lane,ara_test_mask_cycle,pushes);
        pushes++; pending=1;
        $display("ARA_TEST_MASK_LANE_PUSH lane=%0d cycle=%0d serial=%0d pevalid=%0d id=%0d vd=%0d vfu=%0d sew=%0d vl=%0d vstart=%0d start=%0d count=%0d eew=%0d vs=%0d scale=%0d",
          ara_test_mask_lane,ara_test_mask_cycle,pushes,`ARA_TEST_MASK_L.pe_req_valid,
          `ARA_TEST_MASK_L.pe_req.id,`ARA_TEST_MASK_L.pe_req.vd,`ARA_TEST_MASK_L.pe_req.vfu,
          `ARA_TEST_MASK_L.pe_req.vtype.vsew,`ARA_TEST_MASK_L.pe_req.vl,`ARA_TEST_MASK_L.pe_req.vstart,
          `ARA_TEST_MASK_L.operand_request[ara_pkg::MaskM].vstart,
          `ARA_TEST_MASK_L.operand_request[ara_pkg::MaskM].vl,
          `ARA_TEST_MASK_L.operand_request[ara_pkg::MaskM].eew,
          `ARA_TEST_MASK_L.operand_request[ara_pkg::MaskM].vs,
          `ARA_TEST_MASK_L.operand_request[ara_pkg::MaskM].scale_vl);
      end
      held=`ARA_TEST_MASK_L.operand_request_valid_o[ara_pkg::MaskM] &&
           !`ARA_TEST_MASK_L.operand_request_ready_i[ara_pkg::MaskM];
      held_payload=fixed_payload;
      if (held) stalls++;
    end
  end
  // Actual physical VRF read addresses, independent of the requested start.
  always @(posedge ara_tb.clk) if (ara_tb.rst_n && ara_test_mask_enabled)
    for (int bank=0; bank<8; bank++)
      if (`ARA_TEST_MASK_A.gen_lanes[ara_test_mask_lane].i_lane.vrf_req[bank] &&
          !`ARA_TEST_MASK_A.gen_lanes[ara_test_mask_lane].i_lane.vrf_wen[bank] &&
          `ARA_TEST_MASK_A.gen_lanes[ara_test_mask_lane].i_lane.vrf_tgt_opqueue[bank] == ara_pkg::MaskM)
        $display("ARA_TEST_MASK_VRF_READ cycle=%0d lane=%0d bank=%0d row=%0d",
          ara_test_mask_cycle,ara_test_mask_lane,bank,
          `ARA_TEST_MASK_A.gen_lanes[ara_test_mask_lane].i_lane.vrf_addr[bank]);
  // Real returned data, after the SRAM and crossbar. Pair it with the
  // physical read stream in the offline checker, before operand reuse.
  always @(posedge ara_tb.clk) if (ara_tb.rst_n && ara_test_mask_enabled &&
      `ARA_TEST_MASK_A.gen_lanes[ara_test_mask_lane].i_lane.vrf_operand_valid[ara_pkg::MaskM])
    $display("ARA_TEST_MASK_VRF_RETURN cycle=%0d lane=%0d data_bits=%064b",
      ara_test_mask_cycle,ara_test_mask_lane,
      `ARA_TEST_MASK_A.gen_lanes[ara_test_mask_lane].i_lane.vrf_operand[ara_pkg::MaskM]);
  always @(ara_test_mask_snapshot) if (ara_test_mask_enabled)
    $strobe("ARA_TEST_MASK_LANE_SNAPSHOT lane=%0d time_ns=%0d cycle=%0d pushes=%0d handshakes=%0d stalls=%0d hold_errors=%0d pending=%0d",
             ara_test_mask_lane,ara_test_mask_snapshot_observed_ns,ara_test_mask_cycle,pushes,handshakes,stalls,hold_errors,pending);
  final if (ara_test_mask_enabled)
    $display("ARA_TEST_MASK_LANE_SUMMARY lane=%0d time_ns=%0d cycle=%0d pushes=%0d handshakes=%0d stalls=%0d hold_errors=%0d pending=%0d",
             ara_test_mask_lane,longint'($realtime / 1ns),ara_test_mask_cycle,pushes,handshakes,stalls,hold_errors,pending);
end

always @(posedge ara_tb.clk) begin
  ara_test_mask_cycle <= ara_test_mask_cycle + 1;
  if (ara_tb.rst_n && ara_test_mask_enabled) begin
    // Observe the real two-entry consumer cut, before this edge changes it.
    // Consume before fill in the event list, so same-cycle replacement keeps
    // the independent per-lane FIFO order. No internal generate-name guess.
    if (`ARA_TEST_MASK_D.vinsn_issue_valid && !`ARA_TEST_MASK_D.vinsn_issue_q.vm &&
        `ARA_TEST_MASK_D.mask_ready_d && |`ARA_TEST_MASK_D.mask_valid_q)
      $display("ARA_TEST_MASK_UNIT_CONSUME cycle=%0d load=1 store=0 id=%0d vd=%0d sew=%0d vl=%0d vstart=%0d valid_bits=%02b mask_bits=%016b",
        ara_test_mask_cycle,`ARA_TEST_MASK_D.vinsn_issue_q.id,`ARA_TEST_MASK_D.vinsn_issue_q.vd,
        `ARA_TEST_MASK_D.vinsn_issue_q.vtype.vsew,`ARA_TEST_MASK_D.vinsn_issue_q.vl,`ARA_TEST_MASK_D.vinsn_issue_q.vstart,
        `ARA_TEST_MASK_D.mask_valid_q,`ARA_TEST_MASK_D.mask_q);
    if (`ARA_TEST_MASK_S.vinsn_issue_valid && !`ARA_TEST_MASK_S.vinsn_issue_q.vm &&
        `ARA_TEST_MASK_S.mask_ready_d && |`ARA_TEST_MASK_S.mask_valid_q)
      $display("ARA_TEST_MASK_UNIT_CONSUME cycle=%0d load=0 store=1 id=%0d vd=%0d sew=%0d vl=%0d vstart=%0d valid_bits=%02b mask_bits=%016b",
        ara_test_mask_cycle,`ARA_TEST_MASK_S.vinsn_issue_q.id,`ARA_TEST_MASK_S.vinsn_issue_q.vd,
        `ARA_TEST_MASK_S.vinsn_issue_q.vtype.vsew,`ARA_TEST_MASK_S.vinsn_issue_q.vl,`ARA_TEST_MASK_S.vinsn_issue_q.vstart,
        `ARA_TEST_MASK_S.mask_valid_q,`ARA_TEST_MASK_S.mask_q);
    for (int lane=0;lane<2;lane++) begin
      if (`ARA_TEST_MASK_D.mask_valid_d[lane] && `ARA_TEST_MASK_D.mask_ready_q[lane])
        $display("ARA_TEST_MASK_UNIT_CUT_FILL cycle=%0d load=1 store=0 lane=%0d serial=%0d current_masku_id=%0d id=%0d vd=%0d sew=%0d vl=%0d vstart=%0d byte_bits=%08b",
          ara_test_mask_cycle,lane,ara_test_mask_accepted,`ARA_TEST_MASK_M.vinsn_commit.id,
          `ARA_TEST_MASK_D.vinsn_issue_q.id,`ARA_TEST_MASK_D.vinsn_issue_q.vd,`ARA_TEST_MASK_D.vinsn_issue_q.vtype.vsew,
          `ARA_TEST_MASK_D.vinsn_issue_q.vl,`ARA_TEST_MASK_D.vinsn_issue_q.vstart,`ARA_TEST_MASK_D.mask_i[lane]);
      if (`ARA_TEST_MASK_S.mask_valid_d[lane] && `ARA_TEST_MASK_S.mask_ready_q[lane])
        $display("ARA_TEST_MASK_UNIT_CUT_FILL cycle=%0d load=0 store=1 lane=%0d serial=%0d current_masku_id=%0d id=%0d vd=%0d sew=%0d vl=%0d vstart=%0d byte_bits=%08b",
          ara_test_mask_cycle,lane,ara_test_mask_accepted,`ARA_TEST_MASK_M.vinsn_commit.id,
          `ARA_TEST_MASK_S.vinsn_issue_q.id,`ARA_TEST_MASK_S.vinsn_issue_q.vd,`ARA_TEST_MASK_S.vinsn_issue_q.vtype.vsew,
          `ARA_TEST_MASK_S.vinsn_issue_q.vl,`ARA_TEST_MASK_S.vinsn_issue_q.vstart,`ARA_TEST_MASK_S.mask_i[lane]);
    end
    // This is the actual MASKU accept condition, including ID suppression.
    if (!`ARA_TEST_MASK_M.vinsn_queue_full && `ARA_TEST_MASK_M.pe_req_valid_i &&
        !`ARA_TEST_MASK_M.vinsn_running_q[`ARA_TEST_MASK_M.pe_req_i.id] &&
        !`ARA_TEST_MASK_M.pe_req_i.vm &&
        (`ARA_TEST_MASK_M.pe_req_i.vfu inside {ara_pkg::VFU_LoadUnit,ara_pkg::VFU_StoreUnit})) begin
      ara_test_mask_accepted++;
      $display("ARA_TEST_MASK_UNIT_ACCEPT cycle=%0d serial=%0d id=%0d vd=%0d vfu=%0d sew=%0d mask_eew=%0d vl=%0d vstart=%0d read_next=%0d commit_next=%0d pointer_next=%0d issue_queue=%0d commit_queue=%0d",
        ara_test_mask_cycle,ara_test_mask_accepted,`ARA_TEST_MASK_M.pe_req_i.id,`ARA_TEST_MASK_M.pe_req_i.vd,
        `ARA_TEST_MASK_M.pe_req_i.vfu,`ARA_TEST_MASK_M.pe_req_i.vtype.vsew,
        `ARA_TEST_MASK_M.pe_req_i.eew_vmask,`ARA_TEST_MASK_M.pe_req_i.vl,`ARA_TEST_MASK_M.pe_req_i.vstart,
        `ARA_TEST_MASK_M.read_cnt_d,`ARA_TEST_MASK_M.commit_cnt_d,`ARA_TEST_MASK_M.mask_pnt_d,
        `ARA_TEST_MASK_M.vinsn_queue_q.issue_cnt,`ARA_TEST_MASK_M.vinsn_queue_q.commit_cnt);
    end
    if (!`ARA_TEST_MASK_M.mask_queue_full && `ARA_TEST_MASK_M.vinsn_issue_valid &&
        (`ARA_TEST_MASK_M.vinsn_issue.vfu inside {ara_pkg::VFU_LoadUnit,ara_pkg::VFU_StoreUnit}) &&
        !`ARA_TEST_MASK_M.vinsn_issue.vm && &`ARA_TEST_MASK_M.masku_operand_m_valid) begin
      ara_test_mask_emitted++;
      $display("ARA_TEST_MASK_UNIT_EMIT cycle=%0d serial=%0d id=%0d vd=%0d sew=%0d mask_eew=%0d vl=%0d vstart=%0d read=%0d pointer=%0d operand_bits=%0128b mask_bits=%016b valid_bits=%02b operand_ready_bits=%02b slot=%0d",
        ara_test_mask_cycle,ara_test_mask_accepted,`ARA_TEST_MASK_M.vinsn_issue.id,`ARA_TEST_MASK_M.vinsn_issue.vd,
        `ARA_TEST_MASK_M.vinsn_issue.vtype.vsew,`ARA_TEST_MASK_M.vinsn_issue.eew_vmask,
        `ARA_TEST_MASK_M.vinsn_issue.vl,`ARA_TEST_MASK_M.vinsn_issue.vstart,
        `ARA_TEST_MASK_M.read_cnt_q,`ARA_TEST_MASK_M.mask_pnt_q,`ARA_TEST_MASK_M.masku_operand_m,
        `ARA_TEST_MASK_M.mask_queue_d[`ARA_TEST_MASK_M.mask_queue_write_pnt_q],
        `ARA_TEST_MASK_M.mask_queue_valid_d[`ARA_TEST_MASK_M.mask_queue_write_pnt_q],
        `ARA_TEST_MASK_M.masku_operand_m_ready, `ARA_TEST_MASK_M.mask_queue_write_pnt_q);
    end
    if (`ARA_TEST_MASK_M.vinsn_commit_valid && !`ARA_TEST_MASK_M.vinsn_commit.vm &&
        (`ARA_TEST_MASK_M.vinsn_commit.vfu inside {ara_pkg::VFU_LoadUnit,ara_pkg::VFU_StoreUnit}) &&
        `ARA_TEST_MASK_M.pe_resp.vinsn_done[`ARA_TEST_MASK_M.vinsn_commit.id]) begin
      ara_test_mask_done++;
      $display("ARA_TEST_MASK_UNIT_DONE cycle=%0d serial=%0d id=%0d vd=%0d vl=%0d vstart=%0d queue=%0d",
        ara_test_mask_cycle,ara_test_mask_accepted,`ARA_TEST_MASK_M.vinsn_commit.id,`ARA_TEST_MASK_M.vinsn_commit.vd,
        `ARA_TEST_MASK_M.vinsn_commit.vl,`ARA_TEST_MASK_M.vinsn_commit.vstart,`ARA_TEST_MASK_M.mask_queue_cnt_q);
    end
    if ((`ARA_TEST_MASK_M.vldu_mask_ready_i || `ARA_TEST_MASK_M.vstu_mask_ready_i) && |`ARA_TEST_MASK_M.mask_valid_o)
      $display("ARA_TEST_MASK_UNIT_PREFETCH cycle=%0d serial=%0d id=%0d valid=%0h mask=%04h load=%0d store=%0d queue=%0d slot=%0d",
        ara_test_mask_cycle,ara_test_mask_accepted,`ARA_TEST_MASK_M.vinsn_commit.id,
        `ARA_TEST_MASK_M.mask_valid_o,`ARA_TEST_MASK_M.mask_o,
        `ARA_TEST_MASK_M.vldu_mask_ready_i,`ARA_TEST_MASK_M.vstu_mask_ready_i,`ARA_TEST_MASK_M.mask_queue_cnt_q,`ARA_TEST_MASK_M.mask_queue_read_pnt_q);
  end
end
always @(ara_test_mask_snapshot) if (ara_test_mask_enabled)
  $strobe("ARA_TEST_MASK_UNIT_SNAPSHOT time_ns=%0d cycle=%0d accepted=%0d emitted=%0d completed=%0d queue=%0d issue_queue=%0d commit_queue=%0d read=%0d commit=%0d pointer=%0d operand_valid_bits=%02b ldu_valid=%0d ldu_remaining=%0d ldu_cut_valid_bits=%02b ldu_cut_ready_bits=%02b stu_valid=%0d stu_remaining=%0d stu_cut_valid_bits=%02b stu_cut_ready_bits=%02b",
    ara_test_mask_snapshot_observed_ns,ara_test_mask_cycle,ara_test_mask_accepted,ara_test_mask_emitted,ara_test_mask_done,`ARA_TEST_MASK_M.mask_queue_cnt_q,
    `ARA_TEST_MASK_M.vinsn_queue_q.issue_cnt,`ARA_TEST_MASK_M.vinsn_queue_q.commit_cnt,
    `ARA_TEST_MASK_M.read_cnt_q,`ARA_TEST_MASK_M.commit_cnt_q,`ARA_TEST_MASK_M.mask_pnt_q,`ARA_TEST_MASK_M.masku_operand_m_valid,
    `ARA_TEST_MASK_D.vinsn_issue_valid,`ARA_TEST_MASK_D.issue_cnt_bytes_q,`ARA_TEST_MASK_D.mask_valid_q,`ARA_TEST_MASK_D.mask_ready_q,
    `ARA_TEST_MASK_S.vinsn_issue_valid,`ARA_TEST_MASK_S.issue_cnt_bytes_q,`ARA_TEST_MASK_S.mask_valid_q,`ARA_TEST_MASK_S.mask_ready_q);
final if (ara_test_mask_enabled)
  $display("ARA_TEST_MASK_UNIT_SUMMARY time_ns=%0d cycle=%0d accepted=%0d emitted=%0d completed=%0d queue=%0d issue_queue=%0d commit_queue=%0d read=%0d commit=%0d pointer=%0d operand_valid_bits=%02b ldu_valid=%0d ldu_remaining=%0d ldu_cut_valid_bits=%02b ldu_cut_ready_bits=%02b stu_valid=%0d stu_remaining=%0d stu_cut_valid_bits=%02b stu_cut_ready_bits=%02b",
    longint'($realtime / 1ns),ara_test_mask_cycle,ara_test_mask_accepted,ara_test_mask_emitted,ara_test_mask_done,`ARA_TEST_MASK_M.mask_queue_cnt_q,
    `ARA_TEST_MASK_M.vinsn_queue_q.issue_cnt,`ARA_TEST_MASK_M.vinsn_queue_q.commit_cnt,
    `ARA_TEST_MASK_M.read_cnt_q,`ARA_TEST_MASK_M.commit_cnt_q,`ARA_TEST_MASK_M.mask_pnt_q,`ARA_TEST_MASK_M.masku_operand_m_valid,
    `ARA_TEST_MASK_D.vinsn_issue_valid,`ARA_TEST_MASK_D.issue_cnt_bytes_q,`ARA_TEST_MASK_D.mask_valid_q,`ARA_TEST_MASK_D.mask_ready_q,
    `ARA_TEST_MASK_S.vinsn_issue_valid,`ARA_TEST_MASK_S.issue_cnt_bytes_q,`ARA_TEST_MASK_S.mask_valid_q,`ARA_TEST_MASK_S.mask_ready_q);

`undef ARA_TEST_MASK_D
`undef ARA_TEST_MASK_S
`undef ARA_TEST_MASK_L
`undef ARA_TEST_MASK_M
`undef ARA_TEST_MASK_A

endmodule
