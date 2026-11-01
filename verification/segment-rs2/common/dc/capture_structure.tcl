# SPDX-License-Identifier: Apache-2.0
# Reused from the downstream module check. Prepared with AI assistance.
proc capture_structure {stage out stats} {
    redirect -file $out/${stage}_check_design.rpt {set design_ok [check_design -nosplit]}
    puts $stats "$stage\tcheck_design_status\t$design_ok"
    redirect -file $out/${stage}_check_timing.rpt {set timing_ok [check_timing -include {loops} -nosplit]}
    puts $stats "$stage\tcheck_timing_status\t$timing_ok"
    set latches [all_registers -cells -level_sensitive -include_operator_register]
    set flops [all_registers -cells -edge_triggered -include_operator_register]
    puts $stats "$stage\tlatches\t[sizeof_collection $latches]"
    puts $stats "$stage\tflipflops\t[sizeof_collection $flops]"
    set f [open $out/${stage}_sequential_cells.tsv w]
    puts $f "kind\tcell\tref"
    foreach_in_collection cell $latches {puts $f "latch\t[get_object_name $cell]\t[get_attribute $cell ref_name]"}
    foreach_in_collection cell $flops {puts $f "flipflop\t[get_object_name $cell]\t[get_attribute $cell ref_name]"}
    close $f
    redirect -file $out/${stage}_references.rpt {report_reference -hierarchy -nosplit}
    redirect -file $out/${stage}_cells.rpt {report_cell -nosplit [get_cells -hierarchical *]}
    set refs [list]
    foreach_in_collection cell [get_cells -hierarchical *] {lappend refs [get_attribute $cell ref_name]}
    set f [open $out/${stage}_reference_resolution.tsv w]
    puts $f "reference\tdesigns\tlibrary_cells\tblack_box_attr\tis_black_box_attr"
    set unresolved 0
    set blackboxes 0
    foreach ref [lsort -unique $refs] {
        set designs [get_designs -quiet $ref]
        set libs [get_lib_cells -quiet */$ref]
        set black [get_attribute -quiet $designs black_box]
        set isblack [get_attribute -quiet $designs is_black_box]
        if {[sizeof_collection $designs] == 0 && [sizeof_collection $libs] == 0} {incr unresolved}
        if {$black eq "true" || $black eq "1" || $isblack eq "true" || $isblack eq "1"} {incr blackboxes}
        puts $f "$ref\t[sizeof_collection $designs]\t[sizeof_collection $libs]\t$black\t$isblack"
    }
    close $f
    puts $stats "$stage\traw_no_design_or_library_references\t$unresolved"
    puts $stats "$stage\traw_reference_blackbox_attributes\t$blackboxes"
    puts $stats "$stage\tcells\t[sizeof_collection [get_cells -hierarchical *]]"
    # A no_map operator need not have a get_designs/get_lib_cells object.
    # Classify real cell attributes; an absent attribute is not false.
    redirect -file $out/${stage}_available_cell_attributes.rpt {list_attributes -application -class cell}
    set f [open $out/${stage}_actual_cell_attributes.tsv w]
    puts $f "cell\tref\tis_hierarchical\tis_unmapped\tis_black_box\tis_sequential\tis_combinational\tis_synlib_operator\tdesigns\tlibrary_cells\tclassification"
    set true_unresolved 0
    set actual_blackboxes 0
    foreach_in_collection cell [get_cells -hierarchical *] {
        set ref [get_attribute $cell ref_name]
        set hierarchical [get_attribute -quiet $cell is_hierarchical]
        set unmapped [get_attribute -quiet $cell is_unmapped]
        set blackbox [get_attribute -quiet $cell is_black_box]
        set sequential [get_attribute -quiet $cell is_sequential]
        set combinational [get_attribute -quiet $cell is_combinational]
        set synlib [get_attribute -quiet $cell is_synlib_operator]
        set ndesigns [sizeof_collection [get_designs -quiet $ref]]
        set nlibs [sizeof_collection [get_lib_cells -quiet */$ref]]
        set classification unexplained
        if {$blackbox eq "true" || $blackbox eq "1"} {
            set classification black_box
            incr actual_blackboxes
        } elseif {$blackbox eq "false" || $blackbox eq "0"} {
            if {($hierarchical eq "true" || $hierarchical eq "1") && $ndesigns > 0} {
                set classification resolved_hierarchical
            } elseif {$nlibs > 0} {
                set classification resolved_library
            } elseif {([string first "*GEN*" $ref] == 0) && $unmapped eq "true" && $combinational eq "true" && $sequential eq "false"} {
                set classification generic_combinational
            } elseif {$ref eq "**SEQGEN**" && $unmapped eq "true" && $sequential eq "true" && $combinational eq "false"} {
                set classification generic_sequential
            } elseif {$synlib eq "true" && $unmapped eq "true" && ($combinational eq "true" || $sequential eq "true")} {
                set classification synlib_operator
            }
        }
        if {$classification eq "unexplained"} {incr true_unresolved}
        puts $f [join [list [get_object_name $cell] $ref $hierarchical $unmapped $blackbox $sequential $combinational $synlib $ndesigns $nlibs $classification] "\t"]
    }
    close $f
    puts $stats "$stage\ttrue_unexplained_cells\t$true_unresolved"
    puts $stats "$stage\tactual_blackbox_cells\t$actual_blackboxes"
    flush $stats
}
