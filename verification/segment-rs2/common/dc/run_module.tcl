# SPDX-License-Identifier: Apache-2.0
# Real Ara module check. Prepared with AI assistance.
# Use the locked dependencies. This script does not fetch or modify sources.
set support [file dirname [file normalize [info script]]]
foreach key {V01_DC_OUTPUT V01_DC_SOURCE V01_DC_ARA_ROOT V01_DC_CVA6_ROOT V01_DC_COMMON_CELLS_ROOT V01_DC_AXI_ROOT V01_DC_FPNEW_ROOT} {
    if {![info exists env($key)] || $env($key) eq ""} {error "Missing $key"}
}
set out [file normalize $env(V01_DC_OUTPUT)]
set ara [file normalize $env(V01_DC_ARA_ROOT)]
set cva6 [file normalize $env(V01_DC_CVA6_ROOT)]
set cc [file normalize $env(V01_DC_COMMON_CELLS_ROOT)]
set axi [file normalize $env(V01_DC_AXI_ROOT)]
set fpnew [file normalize $env(V01_DC_FPNEW_ROOT)]
set dut_source [file normalize $env(V01_DC_SOURCE)]
file mkdir $out/work
set_app_var search_path [concat [list $support $cc/include $axi/include \
    $ara/hardware/include $cva6/core/include] [get_app_var search_path]]
set_app_var target_library [list gtech.db]
set_app_var synthetic_library [list standard.sldb]
set_app_var link_library [list * gtech.db standard.sldb]
define_design_lib WORK -path $out/work
set sources [list \
    $cc/src/cf_math_pkg.sv \
    $axi/src/axi_pkg.sv \
    $fpnew/src/fpnew_pkg.sv \
    $cva6/core/include/config_pkg.sv \
    $cva6/core/include/cv64a6_imafdcv_sv39_config_pkg.sv \
    $cva6/core/include/build_config_pkg.sv \
    $cva6/core/include/riscv_pkg.sv \
    $ara/hardware/include/rvv_pkg.sv \
    $ara/hardware/include/ara_pkg.sv]
switch -- $module {
    dispatcher {
        set top ara_dispatcher_dc
        lappend sources $cc/src/popcount.sv $cc/src/delta_counter.sv $cc/src/counter.sv \
            $ara/hardware/src/segment_sequencer.sv
    }
    lane_sequencer {
        set top ara_lane_sequencer_dc
        lappend sources $cc/src/fifo_v3.sv $cc/src/fall_through_register.sv \
            $cc/src/spill_register_flushable.sv $cc/src/spill_register.sv
    }
    masku {
        set top ara_masku_dc
        lappend sources $cc/src/delta_counter.sv $cc/src/fifo_v3.sv $cc/src/popcount.sv \
            $cc/src/lzc.sv $cc/src/spill_register_flushable.sv $cc/src/spill_register.sv \
            $ara/hardware/src/masku/masku_operands.sv
    }
    default {error "Unsupported module $module"}
}
lappend sources $support/ara_dc_types.sv $dut_source $support/${top}.sv
foreach path $sources {if {![file isfile $path]} {error "Missing source $path"}}
set f [open $out/sources.txt w]
foreach path $sources {puts $f $path}
close $f
source $support/capture_structure.tcl
set stats [open $out/structure.tsv w]
puts $stats "stage\tproperty\tvalue"
if {[catch {
    set analyze_ok [analyze -format sverilog -define SYNTHESIS -library WORK $sources]
    puts $stats "setup\tanalyze_status\t$analyze_ok"
    if {!$analyze_ok} {error "Analyze failed"}
    set elaborate_ok [elaborate $top -library WORK]
    puts $stats "setup\telaborate_status\t$elaborate_ok"
    if {!$elaborate_ok} {error "Elaborate failed"}
    current_design $top
    set link_ok [link]
    puts $stats "setup\tlink_status\t$link_ok"
    if {!$link_ok} {error "Link failed"}
    # Reporting clock only. No timing closure or physical claim follows.
    create_clock -name clk_i -period 5 [get_ports clk_i]
    redirect -file $out/design_pre.rpt {report_design}
    redirect -file $out/ports.rpt {report_port -verbose}
    capture_structure pre $out $stats
    write -format verilog -hierarchy -output $out/pre_generic.v
    set compile_ok [compile -no_map]
    puts $stats "compile\tno_map_status\t$compile_ok"
    if {!$compile_ok} {error "compile -no_map failed"}
    capture_structure post $out $stats
    redirect -file $out/design_post.rpt {report_design}
    write -format verilog -hierarchy -output $out/post_generic.v
    write -format ddc -hierarchy -output $out/design.ddc
    set latches [all_registers -cells -level_sensitive -include_operator_register]
    set flops [all_registers -cells -edge_triggered -include_operator_register]
    if {[sizeof_collection $latches] != 0} {error "Unexpected level-sensitive storage"}
    if {[sizeof_collection $flops] == 0} {error "Sequential DUT was optimized away"}
    puts $stats "control\tcompleted\t1"
} failure]} {
    puts $stats "control\terror\t$failure"
    close $stats
    exit 2
}
close $stats
exit 0
