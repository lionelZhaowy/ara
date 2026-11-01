// SPDX-License-Identifier: Apache-2.0
// Public test support prepared with AI assistance. Add as a passive top.
module ara_mask_observer;
  timeunit 1ns; timeprecision 1ps;
// Passive reads only. No bind, DUT ports, drives, force or completion override.
// +V01_MASK_START_OBSERVE enables all target events without truncation.
`define V01_B7_A ara_tb.dut.i_ara_soc.i_system.i_ara
`define V01_B7_M `V01_B7_A.i_masku
`define V01_B7_D `V01_B7_A.i_vlsu.i_vldu
`define V01_B7_S `V01_B7_A.i_vlsu.i_vstu
`define V01_B7_L `V01_B7_A.gen_lanes[v01_b7_lane].i_lane.i_lane_sequencer

bit v01_b7_enabled = 0;
longint unsigned v01_b7_cycle = 0;
longint unsigned v01_b7_accepted = 0, v01_b7_emitted = 0, v01_b7_done = 0;
longint unsigned v01_b7_snapshot_ns=0, v01_b7_snapshot_observed_ns=0;
event v01_b7_snapshot;
initial begin
  if ($test$plusargs("V01_MASK_START_OBSERVE")) begin
    if (!$value$plusargs("V01_MASK_SNAPSHOT_NS=%d",v01_b7_snapshot_ns) ||
        v01_b7_snapshot_ns==0)
      $fatal(1,"V01 passive mask snapshot needs the frozen positive HDL deadline");
    #(v01_b7_snapshot_ns * 1ns);
    // #0 moves behind the active-edge observers. Each $strobe below reads
    // the counters and DUT q-state in the postponed region, after this
    // timestamp's NBA updates. No HDL time advances and no DUT is driven.
    #0;
    v01_b7_snapshot_observed_ns=longint'($realtime / 1ns);
    ->v01_b7_snapshot;
  end
end
initial begin
  v01_b7_enabled = $test$plusargs("V01_MASK_START_OBSERVE");
  if (v01_b7_enabled) begin
    if (`V01_B7_A.NrLanes != 2 || `V01_B7_A.VLEN != 2048 ||
        ara_pkg::MaskuInsnQueueDepth != 1 ||
        $bits(`V01_B7_M.masku_operand_m[0]) != 64 ||
        $bits(`V01_B7_M.pe_req_i.vl) != 12)
      $fatal(1,"V01 mask observer requires actual 2 lanes/VLEN2048/ELEN64/depth1/12-bit VL");
    $display("V01_MASK_UNIT_READY lanes=2 depth=1 vlenbits=12");
  end
end

for (genvar v01_b7_lane=0; v01_b7_lane<2; v01_b7_lane++) begin: gen_v01_b7_lanes
  longint unsigned pushes=0, handshakes=0, stalls=0, hold_errors=0;
  bit pending=0, held=0;
  logic [255:0] held_payload;
  // Hazards are intentionally excluded: the real lane continuously clears
  // hazard bits while a request waits. Its identity/address/control stay fixed.
  wire [255:0] fixed_payload = 256'({
    `V01_B7_L.operand_request_o[ara_pkg::MaskM].id,
    `V01_B7_L.operand_request_o[ara_pkg::MaskM].vs,
    `V01_B7_L.operand_request_o[ara_pkg::MaskM].scale_vl,
    `V01_B7_L.operand_request_o[ara_pkg::MaskM].cvt_resize,
    `V01_B7_L.operand_request_o[ara_pkg::MaskM].is_reduct,
    `V01_B7_L.operand_request_o[ara_pkg::MaskM].is_slide,
    `V01_B7_L.operand_request_o[ara_pkg::MaskM].eew,
    `V01_B7_L.operand_request_o[ara_pkg::MaskM].conv,
    `V01_B7_L.operand_request_o[ara_pkg::MaskM].target_fu,
    `V01_B7_L.operand_request_o[ara_pkg::MaskM].vtype,
    `V01_B7_L.operand_request_o[ara_pkg::MaskM].vl,
    `V01_B7_L.operand_request_o[ara_pkg::MaskM].vstart});
  initial if ($test$plusargs("V01_MASK_START_OBSERVE"))
    $display("V01_MASK_LANE_READY lane=%0d lanes=2 reqbits=%0d vlenbits=%0d",
             v01_b7_lane, $bits(`V01_B7_L.operand_request_o[ara_pkg::MaskM]),
             $bits(`V01_B7_L.pe_req.vl));
  always @(posedge ara_tb.clk) begin
    if (!ara_tb.rst_n) begin pending=0; held=0; end
    else if (v01_b7_enabled) begin
      if (held && !`V01_B7_L.lsu_ex_flush_o &&
          (`V01_B7_L.operand_request_valid_o[ara_pkg::MaskM] !== 1'b1 ||
           fixed_payload !== held_payload)) begin
        hold_errors++;
        $display("V01_MASK_LANE_HOLD_ERROR lane=%0d cycle=%0d",v01_b7_lane,v01_b7_cycle);
      end
      // Retire the old output before recording a same-cycle replacement.
      if (`V01_B7_L.operand_request_valid_o[ara_pkg::MaskM] &&
          `V01_B7_L.operand_request_ready_i[ara_pkg::MaskM] && pending) begin
        handshakes++;
        $display("V01_MASK_LANE_HANDSHAKE lane=%0d cycle=%0d serial=%0d id=%0d start=%0d count=%0d eew=%0d vs=%0d scale=%0d",
          v01_b7_lane,v01_b7_cycle,pushes,
          `V01_B7_L.operand_request_o[ara_pkg::MaskM].id,
          `V01_B7_L.operand_request_o[ara_pkg::MaskM].vstart,
          `V01_B7_L.operand_request_o[ara_pkg::MaskM].vl,
          `V01_B7_L.operand_request_o[ara_pkg::MaskM].eew,
          `V01_B7_L.operand_request_o[ara_pkg::MaskM].vs,
          `V01_B7_L.operand_request_o[ara_pkg::MaskM].scale_vl);
        pending=0;
      end
      if (`V01_B7_L.lsu_ex_flush_o) begin
        if (pending) $display("V01_MASK_LANE_FLUSH lane=%0d cycle=%0d serial=%0d",v01_b7_lane,v01_b7_cycle,pushes);
        pending=0;
      end
      if (`V01_B7_L.operand_request_push[ara_pkg::MaskM] &&
          !`V01_B7_L.pe_req.vm &&
          (`V01_B7_L.pe_req.vfu inside {ara_pkg::VFU_LoadUnit,ara_pkg::VFU_StoreUnit})) begin
        if (pending) $display("V01_MASK_LANE_OVERWRITE lane=%0d cycle=%0d serial=%0d",v01_b7_lane,v01_b7_cycle,pushes);
        pushes++; pending=1;
        $display("V01_MASK_LANE_PUSH lane=%0d cycle=%0d serial=%0d pevalid=%0d id=%0d vd=%0d vfu=%0d sew=%0d vl=%0d vstart=%0d start=%0d count=%0d eew=%0d vs=%0d scale=%0d",
          v01_b7_lane,v01_b7_cycle,pushes,`V01_B7_L.pe_req_valid,
          `V01_B7_L.pe_req.id,`V01_B7_L.pe_req.vd,`V01_B7_L.pe_req.vfu,
          `V01_B7_L.pe_req.vtype.vsew,`V01_B7_L.pe_req.vl,`V01_B7_L.pe_req.vstart,
          `V01_B7_L.operand_request[ara_pkg::MaskM].vstart,
          `V01_B7_L.operand_request[ara_pkg::MaskM].vl,
          `V01_B7_L.operand_request[ara_pkg::MaskM].eew,
          `V01_B7_L.operand_request[ara_pkg::MaskM].vs,
          `V01_B7_L.operand_request[ara_pkg::MaskM].scale_vl);
      end
      held=`V01_B7_L.operand_request_valid_o[ara_pkg::MaskM] &&
           !`V01_B7_L.operand_request_ready_i[ara_pkg::MaskM];
      held_payload=fixed_payload;
      if (held) stalls++;
    end
  end
  // Actual physical VRF read addresses, independent of the requested start.
  always @(posedge ara_tb.clk) if (ara_tb.rst_n && v01_b7_enabled)
    for (int bank=0; bank<8; bank++)
      if (`V01_B7_A.gen_lanes[v01_b7_lane].i_lane.vrf_req[bank] &&
          !`V01_B7_A.gen_lanes[v01_b7_lane].i_lane.vrf_wen[bank] &&
          `V01_B7_A.gen_lanes[v01_b7_lane].i_lane.vrf_tgt_opqueue[bank] == ara_pkg::MaskM)
        $display("V01_MASK_VRF_READ cycle=%0d lane=%0d bank=%0d row=%0d",
          v01_b7_cycle,v01_b7_lane,bank,
          `V01_B7_A.gen_lanes[v01_b7_lane].i_lane.vrf_addr[bank]);
  always @(v01_b7_snapshot) if (v01_b7_enabled)
    $strobe("V01_MASK_LANE_SNAPSHOT lane=%0d time_ns=%0d cycle=%0d pushes=%0d handshakes=%0d stalls=%0d hold_errors=%0d pending=%0d",
             v01_b7_lane,v01_b7_snapshot_observed_ns,v01_b7_cycle,pushes,handshakes,stalls,hold_errors,pending);
  final if (v01_b7_enabled)
    $display("V01_MASK_LANE_SUMMARY lane=%0d time_ns=%0d cycle=%0d pushes=%0d handshakes=%0d stalls=%0d hold_errors=%0d pending=%0d",
             v01_b7_lane,longint'($realtime / 1ns),v01_b7_cycle,pushes,handshakes,stalls,hold_errors,pending);
end

always @(posedge ara_tb.clk) begin
  v01_b7_cycle <= v01_b7_cycle + 1;
  if (ara_tb.rst_n && v01_b7_enabled) begin
    // Observe the real two-entry consumer cut, before this edge changes it.
    // Consume before fill in the event list, so same-cycle replacement keeps
    // the independent per-lane FIFO order. No internal generate-name guess.
    if (`V01_B7_D.vinsn_issue_valid && !`V01_B7_D.vinsn_issue_q.vm &&
        `V01_B7_D.mask_ready_d && |`V01_B7_D.mask_valid_q)
      $display("V01_MASK_UNIT_CONSUME cycle=%0d load=1 store=0 id=%0d vd=%0d sew=%0d vl=%0d vstart=%0d valid_bits=%02b mask_bits=%016b",
        v01_b7_cycle,`V01_B7_D.vinsn_issue_q.id,`V01_B7_D.vinsn_issue_q.vd,
        `V01_B7_D.vinsn_issue_q.vtype.vsew,`V01_B7_D.vinsn_issue_q.vl,`V01_B7_D.vinsn_issue_q.vstart,
        `V01_B7_D.mask_valid_q,`V01_B7_D.mask_q);
    if (`V01_B7_S.vinsn_issue_valid && !`V01_B7_S.vinsn_issue_q.vm &&
        `V01_B7_S.mask_ready_d && |`V01_B7_S.mask_valid_q)
      $display("V01_MASK_UNIT_CONSUME cycle=%0d load=0 store=1 id=%0d vd=%0d sew=%0d vl=%0d vstart=%0d valid_bits=%02b mask_bits=%016b",
        v01_b7_cycle,`V01_B7_S.vinsn_issue_q.id,`V01_B7_S.vinsn_issue_q.vd,
        `V01_B7_S.vinsn_issue_q.vtype.vsew,`V01_B7_S.vinsn_issue_q.vl,`V01_B7_S.vinsn_issue_q.vstart,
        `V01_B7_S.mask_valid_q,`V01_B7_S.mask_q);
    for (int lane=0;lane<2;lane++) begin
      if (`V01_B7_D.mask_valid_d[lane] && `V01_B7_D.mask_ready_q[lane])
        $display("V01_MASK_UNIT_CUT_FILL cycle=%0d load=1 store=0 lane=%0d serial=%0d current_masku_id=%0d id=%0d vd=%0d sew=%0d vl=%0d vstart=%0d byte_bits=%08b",
          v01_b7_cycle,lane,v01_b7_accepted,`V01_B7_M.vinsn_commit.id,
          `V01_B7_D.vinsn_issue_q.id,`V01_B7_D.vinsn_issue_q.vd,`V01_B7_D.vinsn_issue_q.vtype.vsew,
          `V01_B7_D.vinsn_issue_q.vl,`V01_B7_D.vinsn_issue_q.vstart,`V01_B7_D.mask_i[lane]);
      if (`V01_B7_S.mask_valid_d[lane] && `V01_B7_S.mask_ready_q[lane])
        $display("V01_MASK_UNIT_CUT_FILL cycle=%0d load=0 store=1 lane=%0d serial=%0d current_masku_id=%0d id=%0d vd=%0d sew=%0d vl=%0d vstart=%0d byte_bits=%08b",
          v01_b7_cycle,lane,v01_b7_accepted,`V01_B7_M.vinsn_commit.id,
          `V01_B7_S.vinsn_issue_q.id,`V01_B7_S.vinsn_issue_q.vd,`V01_B7_S.vinsn_issue_q.vtype.vsew,
          `V01_B7_S.vinsn_issue_q.vl,`V01_B7_S.vinsn_issue_q.vstart,`V01_B7_S.mask_i[lane]);
    end
    // This is the actual MASKU accept condition, including ID suppression.
    if (!`V01_B7_M.vinsn_queue_full && `V01_B7_M.pe_req_valid_i &&
        !`V01_B7_M.vinsn_running_q[`V01_B7_M.pe_req_i.id] &&
        !`V01_B7_M.pe_req_i.vm &&
        (`V01_B7_M.pe_req_i.vfu inside {ara_pkg::VFU_LoadUnit,ara_pkg::VFU_StoreUnit})) begin
      v01_b7_accepted++;
      $display("V01_MASK_UNIT_ACCEPT cycle=%0d serial=%0d id=%0d vd=%0d vfu=%0d sew=%0d mask_eew=%0d vl=%0d vstart=%0d read_next=%0d commit_next=%0d pointer_next=%0d issue_queue=%0d commit_queue=%0d",
        v01_b7_cycle,v01_b7_accepted,`V01_B7_M.pe_req_i.id,`V01_B7_M.pe_req_i.vd,
        `V01_B7_M.pe_req_i.vfu,`V01_B7_M.pe_req_i.vtype.vsew,
        `V01_B7_M.pe_req_i.eew_vmask,`V01_B7_M.pe_req_i.vl,`V01_B7_M.pe_req_i.vstart,
        `V01_B7_M.read_cnt_d,`V01_B7_M.commit_cnt_d,`V01_B7_M.mask_pnt_d,
        `V01_B7_M.vinsn_queue_q.issue_cnt,`V01_B7_M.vinsn_queue_q.commit_cnt);
    end
    if (!`V01_B7_M.mask_queue_full && `V01_B7_M.vinsn_issue_valid &&
        (`V01_B7_M.vinsn_issue.vfu inside {ara_pkg::VFU_LoadUnit,ara_pkg::VFU_StoreUnit}) &&
        !`V01_B7_M.vinsn_issue.vm && &`V01_B7_M.masku_operand_m_valid) begin
      v01_b7_emitted++;
      $display("V01_MASK_UNIT_EMIT cycle=%0d serial=%0d id=%0d vd=%0d sew=%0d mask_eew=%0d vl=%0d vstart=%0d read=%0d pointer=%0d operand_bits=%0128b mask_bits=%016b valid_bits=%02b slot=%0d",
        v01_b7_cycle,v01_b7_accepted,`V01_B7_M.vinsn_issue.id,`V01_B7_M.vinsn_issue.vd,
        `V01_B7_M.vinsn_issue.vtype.vsew,`V01_B7_M.vinsn_issue.eew_vmask,
        `V01_B7_M.vinsn_issue.vl,`V01_B7_M.vinsn_issue.vstart,
        `V01_B7_M.read_cnt_q,`V01_B7_M.mask_pnt_q,`V01_B7_M.masku_operand_m,
        `V01_B7_M.mask_queue_d[`V01_B7_M.mask_queue_write_pnt_q],
        `V01_B7_M.mask_queue_valid_d[`V01_B7_M.mask_queue_write_pnt_q], `V01_B7_M.mask_queue_write_pnt_q);
    end
    if (`V01_B7_M.vinsn_commit_valid && !`V01_B7_M.vinsn_commit.vm &&
        (`V01_B7_M.vinsn_commit.vfu inside {ara_pkg::VFU_LoadUnit,ara_pkg::VFU_StoreUnit}) &&
        `V01_B7_M.pe_resp.vinsn_done[`V01_B7_M.vinsn_commit.id]) begin
      v01_b7_done++;
      $display("V01_MASK_UNIT_DONE cycle=%0d serial=%0d id=%0d vd=%0d vl=%0d vstart=%0d queue=%0d",
        v01_b7_cycle,v01_b7_accepted,`V01_B7_M.vinsn_commit.id,`V01_B7_M.vinsn_commit.vd,
        `V01_B7_M.vinsn_commit.vl,`V01_B7_M.vinsn_commit.vstart,`V01_B7_M.mask_queue_cnt_q);
    end
    if ((`V01_B7_M.vldu_mask_ready_i || `V01_B7_M.vstu_mask_ready_i) && |`V01_B7_M.mask_valid_o)
      $display("V01_MASK_UNIT_PREFETCH cycle=%0d serial=%0d id=%0d valid=%0h mask=%04h load=%0d store=%0d queue=%0d slot=%0d",
        v01_b7_cycle,v01_b7_accepted,`V01_B7_M.vinsn_commit.id,
        `V01_B7_M.mask_valid_o,`V01_B7_M.mask_o,
        `V01_B7_M.vldu_mask_ready_i,`V01_B7_M.vstu_mask_ready_i,`V01_B7_M.mask_queue_cnt_q,`V01_B7_M.mask_queue_read_pnt_q);
  end
end
always @(v01_b7_snapshot) if (v01_b7_enabled)
  $strobe("V01_MASK_UNIT_SNAPSHOT time_ns=%0d cycle=%0d accepted=%0d emitted=%0d completed=%0d queue=%0d issue_queue=%0d commit_queue=%0d read=%0d commit=%0d pointer=%0d operand_valid_bits=%02b ldu_valid=%0d ldu_remaining=%0d ldu_cut_valid_bits=%02b ldu_cut_ready_bits=%02b stu_valid=%0d stu_remaining=%0d stu_cut_valid_bits=%02b stu_cut_ready_bits=%02b",
    v01_b7_snapshot_observed_ns,v01_b7_cycle,v01_b7_accepted,v01_b7_emitted,v01_b7_done,`V01_B7_M.mask_queue_cnt_q,
    `V01_B7_M.vinsn_queue_q.issue_cnt,`V01_B7_M.vinsn_queue_q.commit_cnt,
    `V01_B7_M.read_cnt_q,`V01_B7_M.commit_cnt_q,`V01_B7_M.mask_pnt_q,`V01_B7_M.masku_operand_m_valid,
    `V01_B7_D.vinsn_issue_valid,`V01_B7_D.issue_cnt_bytes_q,`V01_B7_D.mask_valid_q,`V01_B7_D.mask_ready_q,
    `V01_B7_S.vinsn_issue_valid,`V01_B7_S.issue_cnt_bytes_q,`V01_B7_S.mask_valid_q,`V01_B7_S.mask_ready_q);
final if (v01_b7_enabled)
  $display("V01_MASK_UNIT_SUMMARY time_ns=%0d cycle=%0d accepted=%0d emitted=%0d completed=%0d queue=%0d issue_queue=%0d commit_queue=%0d read=%0d commit=%0d pointer=%0d operand_valid_bits=%02b ldu_valid=%0d ldu_remaining=%0d ldu_cut_valid_bits=%02b ldu_cut_ready_bits=%02b stu_valid=%0d stu_remaining=%0d stu_cut_valid_bits=%02b stu_cut_ready_bits=%02b",
    longint'($realtime / 1ns),v01_b7_cycle,v01_b7_accepted,v01_b7_emitted,v01_b7_done,`V01_B7_M.mask_queue_cnt_q,
    `V01_B7_M.vinsn_queue_q.issue_cnt,`V01_B7_M.vinsn_queue_q.commit_cnt,
    `V01_B7_M.read_cnt_q,`V01_B7_M.commit_cnt_q,`V01_B7_M.mask_pnt_q,`V01_B7_M.masku_operand_m_valid,
    `V01_B7_D.vinsn_issue_valid,`V01_B7_D.issue_cnt_bytes_q,`V01_B7_D.mask_valid_q,`V01_B7_D.mask_ready_q,
    `V01_B7_S.vinsn_issue_valid,`V01_B7_S.issue_cnt_bytes_q,`V01_B7_S.mask_valid_q,`V01_B7_S.mask_ready_q);

`undef V01_B7_D
`undef V01_B7_S
`undef V01_B7_L
`undef V01_B7_M
`undef V01_B7_A

endmodule
