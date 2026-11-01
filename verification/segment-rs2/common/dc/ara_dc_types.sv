// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
// Original author: Matheus Cavalcante <matheusd@iis.ee.ethz.ch>.
// Test-only extraction and wrapper adaptation prepared with AI assistance.
// Ara source pin: 34bd3bc152421b4601a7bf3d6e8e91ffb545c99e.
`include "ara/intf_typedef.svh"
package ara_dc_types_pkg;
  import ara_pkg::*;
  localparam int unsigned NrLanes = 2;
  localparam int unsigned VLEN = 2048;
  localparam int unsigned AxiAddrWidth = 64;
  localparam int unsigned AxiNarrowDataWidth = 64;
  localparam int unsigned AxiIdWidth = 5;
  localparam int unsigned AxiUserWidth = 1;
  localparam fpu_support_e FPUSupport = FPUSupportHalfSingleDouble;
  localparam logic [63:0] DRAMBase = 64'h8000_0000;
  localparam logic [63:0] DRAMLength = 64'h4000_0000;
  localparam logic [63:0] UARTBase = 64'hc000_0000;
  localparam logic [63:0] UARTLength = 64'h1000;
  localparam logic [63:0] CTRLBase = 64'hd000_0000;
  localparam logic [63:0] CTRLLength = 64'h1000;

  // Verbatim from ara_soc.sv; constants above match ara_tb/ara_testharness.
  function automatic config_pkg::cva6_user_cfg_t gen_usr_cva6_config(config_pkg::cva6_user_cfg_t cfg);
    cfg.AxiAddrWidth          = AxiAddrWidth;
    cfg.AxiDataWidth          = AxiNarrowDataWidth;
    cfg.AxiIdWidth            = AxiIdWidth;
    cfg.AxiUserWidth          = AxiUserWidth;
    cfg.XF16                  = FPUSupport[3];
    cfg.RVF                   = FPUSupport[4];
    cfg.RVD                   = FPUSupport[5];
    cfg.XF16ALT               = FPUSupport[2];
    cfg.XF8                   = FPUSupport[1];
//  cfg.XF8ALT                = FPUSupport[0]; // Not supported by OpenHW Group's CVFPU
    cfg.NrPMPEntries          = 0;
    // idempotent region
    cfg.NrNonIdempotentRules  = 2;
    cfg.NonIdempotentAddrBase = {UARTBase, CTRLBase};
    cfg.NonIdempotentLength   = {UARTLength, CTRLLength};
    cfg.NrExecuteRegionRules  = 3;
    //                          DRAM;       Boot ROM;   Debug Module
    cfg.ExecuteRegionAddrBase = {DRAMBase,   64'h1_0000, 64'h0};
    cfg.ExecuteRegionLength   = {DRAMLength, 64'h10000,  64'h1000};
    // cached region
    cfg.NrCachedRegionRules   = 1;
    cfg.CachedRegionAddrBase  = {DRAMBase};
    cfg.CachedRegionLength    = {DRAMLength};
    // Return modified config
    return cfg;
  endfunction
  localparam config_pkg::cva6_user_cfg_t CVA6AraConfig_user =
      gen_usr_cva6_config(cva6_config_pkg::cva6_cfg);
  localparam config_pkg::cva6_cfg_t CVA6Cfg =
      build_config_pkg::build_config(CVA6AraConfig_user);
  typedef logic [$clog2(VLEN+1)-1:0] vlen_t;
  localparam int unsigned DataWidth = $bits(elen_t);
  localparam int unsigned StrbWidth = DataWidth / 8;
  typedef logic [StrbWidth-1:0] strb_t;
  localparam int unsigned VRFBSizePerLane = (VLEN / 8 / NrLanes) * 32;
  typedef logic [cf_math_pkg::idx_width(VRFBSizePerLane)-1:0] vaddr_t;
  `CVA6_TYPEDEF_EXCEPTION(exception_t, CVA6Cfg)
  `CVA6_INTF_TYPEDEF_ACC_REQ(accelerator_req_t, CVA6Cfg, fpnew_pkg::roundmode_e)
  `CVA6_INTF_TYPEDEF_ACC_RESP(accelerator_resp_t, CVA6Cfg, exception_t)
  // pe_req_t and pe_resp_t use the unmodified public Ara include.
  `include "ara/ara_typedef.svh"

  // Verbatim dispatcher request/response types from ara.sv.
  typedef struct packed {
    ara_op_e op; // Operation

    // Stores and slides do not re-shuffle the
    // source registers. In these two cases, vl refers
    // to the target EEW and vtype.vsew, respectively.
    // Since operand requesters work with the old
    // eew of the source registers, we should rescale
    // vl to the old eew to fetch the correct number of Bytes.
    //
    // Another solution would be to pass directly the target
    // eew (vstores) or the vtype.vsew (vslides), but this would
    // create confusion with the current naming convention
    logic scale_vl;

    // Mask vector register operand
    logic vm;
    rvv_pkg::vew_e eew_vmask;

    // 1st vector register operand
    logic [4:0] vs1;
    logic use_vs1;
    opqueue_conversion_e conversion_vs1;
    rvv_pkg::vew_e eew_vs1;
    rvv_pkg::vew_e old_eew_vs1;

    // 2nd vector register operand
    logic [4:0] vs2;
    logic use_vs2;
    opqueue_conversion_e conversion_vs2;
    rvv_pkg::vew_e eew_vs2;

    // Use vd as an operand as well (e.g., vmacc)
    logic use_vd_op;
    rvv_pkg::vew_e eew_vd_op;

    // Scalar operand
    elen_t scalar_op;
    logic use_scalar_op;

    // 2nd scalar operand: stride for constant-strided vector load/stores, slide offset for vector
    // slides
    elen_t stride;
    logic is_stride_np2;

    // Destination vector register
    logic [4:0] vd;
    logic use_vd;

    // If asserted: vs2 is kept in MulFPU opqueue C, and vd_op in MulFPU A
    logic swap_vs2_vd_op;

    // Effective length multiplier
    rvv_pkg::vlmul_e emul;

    // Number of segments in segment mem op
    logic [2:0] nf;

    // Is this a fault-only-first load?
    logic fault_only_first;

    // Rounding-Mode for FP operations
    fpnew_pkg::roundmode_e fp_rm;
    // Widen FP immediate (re-encoding)
    logic wide_fp_imm;
    // Resizing of FP conversions
    resize_e cvt_resize;

    // Vector machine metadata
    vlen_t vl;
    vlen_t vstart;
    rvv_pkg::vtype_t vtype;

    // Request token, for registration in the sequencer
    logic token;
  } ara_req_t;

  typedef struct packed {
    // Scalar response
    elen_t resp;

    // Instruction triggered an exception
    exception_t exception;

    // Fault-only-first exception on element whose idx > 0
    logic fof_exception;

    // New value for vstart
    vlen_t exception_vstart;
  } ara_resp_t;

  // Verbatim lane types from lane.sv.
  typedef struct packed {
    vid_t id; // ID of the vector instruction

    logic [4:0] vs; // Vector register operand

    logic scale_vl; // Rescale vl taking into account the new and old EEW

    resize_e cvt_resize;    // Resizing of FP conversions

    logic is_reduct; // Is this a reduction?
    logic is_slide; // Is this a slide?

    rvv_pkg::vew_e eew;        // Effective element width
    opqueue_conversion_e conv; // Type conversion

    target_fu_e target_fu;     // Target FU of the opqueue (if it is not clear)

    // Vector machine metadata
    rvv_pkg::vtype_t vtype;
    vlen_t vl;
    vlen_t vstart;

    // Hazards
    logic [NrVInsn-1:0] hazard;
  } operand_request_cmd_t;

  typedef struct packed {
    rvv_pkg::vew_e eew;        // Effective element width
    vlen_t elem_count;         // Vector body length
    opqueue_conversion_e conv; // Type conversion
    logic [1:0] ntr_red;       // Neutral type for reductions
    logic is_reduct;           // Is this a reduction?
    target_fu_e target_fu;     // Target FU of the opqueue (if it is not clear)
  } operand_queue_cmd_t;

  // This is the interface between the lane's sequencer and the lane's VFUs.
  typedef struct packed {
    vid_t id; // ID of the vector instruction

    ara_op_e op; // Operation
    logic vm;    // Masked instruction

    logic use_vs1;   // This operation uses vs1
    logic use_vs2;   // This operation uses vs1
    logic use_vd_op; // This operation uses vd as an operand as well

    elen_t scalar_op;    // Scalar operand
    logic use_scalar_op; // This operation uses the scalar operand

    vfu_e vfu; // VFU responsible for this instruction

    logic [4:0] vd; // Vector destination register
    logic use_vd;

    logic swap_vs2_vd_op; // If asserted: vs2 is kept in MulFPU opqueue C, and vd_op in MulFPU A

    fpnew_pkg::roundmode_e fp_rm; // Rounding-Mode for FP operations
    logic wide_fp_imm;            // Widen FP immediate (re-encoding)
    resize_e cvt_resize;    // Resizing of FP conversions

    // Vector machine metadata
    vlen_t vl;
    vlen_t vstart;
    rvv_pkg::vtype_t vtype;
  } vfu_operation_t;
endpackage
