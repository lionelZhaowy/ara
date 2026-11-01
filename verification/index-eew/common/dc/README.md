# Fixed Ara module checks

These files prepare real module checks for Ara
`34bd3bc152421b4601a7bf3d6e8e91ffb545c99e`, two lanes, VLEN=2048, ELEN=64.
The wrappers expose every DUT port. They use no stubs or functional tie-offs.
The initial local preparation did not run DC. Two real lane-sequencer calls for this comparison later completed and were accepted. See [the actual lane results](../../batch5/REPRODUCTION.md#lane-implementation-checks). Lane and MASKU results are tracked separately while the remaining checks run.

`ara_dc_types.sv` retains the exact request/response structs from `ara.sv`,
the exact three lane structs from `lane.sv`, and the exact configuration
function from `ara_soc.sv`. Its constants match `ara_tb` and `ara_testharness`:
AXI address/data width 64, AXI ID width 5, AXI user width 1, and no PMP entries.
The function derives the real `CVA6Cfg` from `cv64a6_imafdcv_sv39_config_pkg`
through the unchanged `build_config_pkg`. It does not use `cheshire_pkg`.
PE structs come from the official `ara/ara_typedef.svh` include.

Use these unchanged dependency commits from the Ara lockfile:

| Dependency | Commit |
| --- | --- |
| CVA6 | `99eac9a649001bdf5b8f9da52e0ca73d5c48db1c` |
| common_cells | `c27bce39ebb2e6bae52f60960814a2afca7bd4cb` |
| AXI | `853ede23b2a9837951b74dbdc6d18c3eef5bac7d` |
| FPnew | `e5aa6a01b5bbe1675c3aa8872e1203413ded83d1` |

Set `V01_DC_ARA_ROOT`, `V01_DC_CVA6_ROOT`, `V01_DC_COMMON_CELLS_ROOT`,
`V01_DC_AXI_ROOT`, and `V01_DC_FPNEW_ROOT` to those existing source roots.
Set `V01_DC_SOURCE` to the exact compared DUT file and `V01_DC_OUTPUT` to
a separate absolute output directory for each design. Retain the source
hashes and original tool logs with each result. Use the installed DC library
search path for `gtech.db` and `standard.sldb`.

Call the existing tool directly:

```sh
dc_shell -f check_dispatcher.tcl
dc_shell -f check_lane_sequencer.tcl
dc_shell -f check_masku.tcl
```

Run only the required entry for each compared file. The ten distinct designs
are dispatcher O/D, lane O/I/L/A/I+L, and MASKU O/M/A. A separate DC process
and WORK directory prevent design/library reuse between variants.

The script runs analyze, elaborate, link, pre/post structure reports, and
`compile -no_map`. A 5 ns clock supports reporting; this is not a timing target
or timing closure claim. Inspect the original warnings, loop reports, and
actual cell attributes. A missing `get_designs` entry alone does not make
a generic operator unresolved. The reused `capture_structure.tcl` preserves
both the raw reference lookup and the actual property classification.
Tool exit zero alone does not accept unexplained cells, black boxes, or loops.

The hardware files retain the original Solderpad notices and authors.
The Tcl support uses Apache-2.0. AI tools assisted with extraction, adaptation,
and static review. The tool operator must check actual elaboration and reports.
