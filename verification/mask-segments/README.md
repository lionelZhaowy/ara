# Masked segment-memory comparison on official Ara

The tests compare fixed official Ara base `34bd3bc152421b4601a7bf3d6e8e91ffb545c99e`, David Harris's complete proposal, and separate local MASKU/lane changes.
The official configuration is `2_lanes`, VLEN=2048, ELEN=64, WT D-cache, and zero PMP entries.
Architectural `vstart` remains zero. Segment expansion produces the internal start offsets.

See [the complete input and reproduction description](cases/REPRODUCTION.md), [commands](COMMANDS.md), and [recorded results](cases/results.json).
The four inputs execute original-mask, inverse-mask, and unmasked phases in one run.
The observers are passive. They read real DUT state and testbench memory without driving the DUT.

The comparison follows [issue #529](https://github.com/pulp-platform/ara/issues/529) and [PR #468](https://github.com/pulp-platform/ara/pull/468).
The shared alignment method overlaps with David Harris's proposal. It is not unique local work.
The local lane start subset originally came from PR #468 and issue #462.

The RTL patches retain the upstream source copyright and Solderpad-0.51 license.
The new C input, passive observers, and Python checkers use Apache-2.0.
No test source from the unlicensed author reproducer repository is included.

AI tools assisted with implementation, adaptation, execution, result checks, and this documentation.
