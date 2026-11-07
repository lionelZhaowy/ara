#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# AI-assisted passive-event checker. No private paths or run results are embedded.
"""Track actual VRF reads, emitted slots, mask cuts, and consumer owners.

Use the matching public observer. The checker never treats current_masku_id or
the observer's current serial as a payload owner. An EMIT creates that owner.
Slot order and actual per-lane cut handshakes carry it to each CONSUME.
Native completion, signatures, software counts, and assertions need separate
review. Diagnostic mode records semantic differences without a legacy profile.
"""
import argparse
from collections import Counter, defaultdict, deque
import hashlib
import json
from pathlib import Path
import re


# Independently tabulated logical-byte to physical-byte maps for two lanes.
# The --ara-pkg check compares all entries with the actual fixed source table.
LAYOUT = {
    8: (0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15),
    16: (0, 1, 8, 9, 4, 5, 12, 13, 2, 3, 10, 11, 6, 7, 14, 15),
    32: (0, 1, 2, 3, 8, 9, 10, 11, 4, 5, 6, 7, 12, 13, 14, 15),
    64: tuple(range(16)),
}
META = ("id", "vd", "sew", "vl", "vstart")
PHASE_BEGIN = "V01_MEMGAP_PHASE_BEGIN"
PHASE_DONE = "V01_MEMGAP_PHASE_DONE"


def interval(sew, start, end):
    if sew not in LAYOUT or not 0 <= start < end:
        raise ValueError("Unsupported or empty internal active interval")
    payload = 16 // (sew // 8)
    packets = [first for first in range(0, end, payload) if first + payload > start]
    rows = sorted({element // 128 for element in range(start, end)})
    return {"payload": payload, "packets": packets, "rows": rows,
            "remaining": end - packets[0], "pointer": packets[0] % 128}


def mask_byte(nf, phase, byte):
    original = 0xa6 if nf == 5 or byte < 16 else 0x59 if byte < 32 else 0xc9
    return original if phase == 0 else original ^ 255 if phase == 1 else 255


def architectural_strobe(nf, phase, first, sew):
    value = 0
    width = sew // 8
    for element in range(16 // width):
        index = first + element
        enabled = (mask_byte(nf, phase, index // 8) >> (index % 8)) & 1
        for subbyte in range(width):
            value |= enabled << LAYOUT[sew][element * width + subbyte]
    return value


def physical_vrf_word(nf, phase, word, lane):
    register, within = divmod(word, 16)
    if register in (1, 2, 3, 4):
        return int.from_bytes(bytes([(0x17, 0x3b, 0x6d, 0xe1)[register - 1]]) * 8, "little")
    if register != 0:
        return None
    packed = bytearray(16)
    for byte in range(16):
        packed[LAYOUT[8][byte]] = mask_byte(nf, phase, within * 16 + byte)
    return int.from_bytes(packed[lane * 8:lane * 8 + 8], "little")


def four_state(bits, width):
    if len(bits) != width or re.fullmatch(r"[01xXzZ]+", bits) is None:
        raise ValueError("Missing complete four-state binary field of width %d" % width)
    value = known = 0
    for bit in bits.lower():
        value <<= 1
        known <<= 1
        if bit in "01":
            value |= int(bit)
            known |= 1
    return value, known


def sampled_strobe(operand, known, pointer, sew, mask_eew):
    value = valid = 0
    for element in range(16 // (sew // 8)):
        logical = pointer + element
        if logical >= 128:
            raise ValueError("Sampled mask pointer crosses its actual operand")
        byte, bit = divmod(logical, 8)
        source = LAYOUT[8 << mask_eew][byte] * 8 + bit
        for subbyte in range(sew // 8):
            dest = LAYOUT[sew][element * (sew // 8) + subbyte]
            value |= ((operand >> source) & 1) << dest
            valid |= ((known >> source) & 1) << dest
    return value, valid


def table_binding(path):
    raw = path.read_bytes()
    text = raw.decode()
    start = text.index("2: unique case (ew)", text.index("function automatic", text.index("shuffle_index") - 100))
    end = text.index("default: begin", start)
    body = text[start:end]
    for sew, wanted in LAYOUT.items():
        match = re.search(r"rvv_pkg::EW%d:\s*begin(.*?)return idx" % sew, body, re.S)
        if match is None:
            raise ValueError("The actual package lacks the two-lane shuffle table")
        pairs = {int(i): int(v) for i, v in re.findall(r"idx\[(\d+)\]\s*=\s*(\d+)", match[1])}
        if tuple(pairs.get(i) for i in range(16)) != wanted:
            raise ValueError("Independent layout table differs from actual ara_pkg for SEW%d" % sew)
    return {"sha256": hashlib.sha256(raw).hexdigest(), "checked_entries": 64}


def parse_log(path):
    rows = []
    phase = None
    for number, line in enumerate(path.read_text(errors="replace").splitlines(), 1):
        match = re.search(r"\b(V01_MASK_[A-Z_]+|V01_MEMGAP_PHASE_BEGIN|V01_MEMGAP_PHASE_DONE|V01_MEMGAP_ENVIRONMENT_FAIL|V01_MEMGAP_DONE)\s+(.*)", line)
        if not match:
            continue
        row = {"event": match[1], "line": number, "phase": phase}
        for key, value in re.findall(r"(\w+)=([^\s]+)", match[2]):
            row[key] = value if key.endswith("_bits") or key in ("mask", "valid", "level") else int(value)
        if row["event"] == PHASE_BEGIN:
            phase = row["phase"]
        row["phase"] = phase if row["event"] != PHASE_DONE else row["phase"]
        rows.append(row)
    return rows


class Review:
    def __init__(self, nf, family, incomplete):
        self.nf, self.family, self.incomplete = nf, family, incomplete
        self.sew, self.vl = (64, 3) if nf == 5 else (8, 129)
        self.vfu = 4 if family == 1 else 5
        self.errors = []
        self.differences = defaultdict(list)
        self.difference_counts = Counter()
        self.owners = []
        self.slots = {}
        self.order = deque()
        self.cuts = {(direction, lane): deque() for direction in ("load", "store") for lane in (0, 1)}
        self.vrf_requests = {lane: deque() for lane in (0, 1)}
        self.vrf_returns = {lane: deque() for lane in (0, 1)}
        self.vrf_data = {lane: deque() for lane in (0, 1)}
        self.lane_pending = {}
        self.lane_owner = {}
        self.pushes = Counter()
        self.handshakes = Counter()
        self.fills = Counter()
        self.consumes = Counter()
        self.emits = Counter()
        self.operand_pops = Counter()
        self.done = set()
        self.transition_states = []
        self.window = (0, float("inf"))
        self.outside_window_events = Counter()

    def outside(self, row):
        return not self.window[0] <= row["line"] <= self.window[1]

    def fail(self, message, row=None):
        self.errors.append({"message": message, "line": None if row is None else row["line"]})

    def difference(self, category, row, **details):
        self.difference_counts[category] += 1
        if len(self.differences[category]) < 20:
            self.differences[category].append(dict(line=row["line"], cycle=row.get("cycle"), **details))

    def origin(self, row, require_sew=True):
        keys = META if require_sew else ("id", "vd", "vl", "vstart")
        candidates = [o for o in self.owners if o["cycle"] <= row["cycle"] and all(row[k] == o[k] for k in keys)]
        if not candidates:
            self.fail("Observed owner has no matching actual MASKU acceptance", row)
            return None
        return candidates[-1]

    def stage_state(self, phase, cycle):
        return {"phase": phase, "cycle": cycle,
                "mask_slots": [{"slot": slot, "owner": self.slots[slot]["owner"], "emitted_phase": self.slots[slot]["phase"]} for slot in self.order],
                "cuts": {"%s_lane%d" % key: [x["owner"] for x in queue] for key, queue in self.cuts.items()}}

    def prepare(self, rows):
        starts = [r["line"] for r in rows if r["event"] == PHASE_BEGIN and r["phase"] == 0]
        ends = [r["line"] for r in rows if r["event"] == PHASE_DONE and r["phase"] == 2]
        if starts:
            self.window = (starts[0], ends[0] if ends else float("inf"))
        ordinals = Counter()
        for row in rows:
            if row["event"] != "V01_MASK_UNIT_ACCEPT":
                continue
            owner = dict(row, owner=len(self.owners))
            phase = owner["phase"]
            ordinal = ordinals[phase]
            ordinals[phase] += 1
            wanted_start = ordinal // self.nf
            if phase not in (0, 1) or ordinal >= self.vl * self.nf or (owner["vl"], owner["vstart"]) != (wanted_start + 1, wanted_start):
                self.difference("accepted_instruction_sequence", row, ordinal=ordinal, observed=[owner["vl"], owner["vstart"]])
            if owner["vfu"] != self.vfu or 8 << owner["sew"] != self.sew:
                self.difference("accepted_instruction_metadata", row)
            if owner["issue_queue"] or owner["commit_queue"]:
                self.fail("Actual depth-one MASKU accepted into a nonempty instruction queue", row)
            model = interval(8 << owner["sew"], owner["vstart"], owner["vl"])
            actual = [owner["read_next"], owner["commit_next"], owner["pointer_next"]]
            wanted = [model["remaining"], model["remaining"], model["pointer"]]
            if actual != wanted:
                self.difference("mask_initialization", row, owner=owner["owner"], observed=actual, wanted=wanted)
            owner["model"] = model
            self.owners.append(owner)

    def lane_push(self, row):
        lane = row["lane"]
        if lane not in (0, 1):
            self.fail("Unexpected physical lane", row)
            return
        if lane in self.lane_pending:
            self.fail("Lane overwrote a pending actual request", row)
        phase = row["phase"]
        candidates = [o for o in self.owners if o["phase"] == phase]
        ordinal = self.pushes[(lane, phase)]
        self.pushes[(lane, phase)] += 1
        origin = candidates[ordinal] if ordinal < len(candidates) else None
        if origin is None or any(row[k] != origin[k] for k in META + ("vfu",)):
            self.fail("Lane push does not match MASKU owner ordinal and metadata", row)
            return
        wanted = [origin["model"]["rows"][0], len(origin["model"]["rows"])]
        actual = [row["start"], row["count"]]
        if actual != wanted:
            self.difference("mask_word_request", row, owner=origin["owner"], lane=lane, observed=actual, wanted=wanted)
        if (row["pevalid"], row["eew"], row["vs"], row["scale"]) != (1, 3, 0, 0):
            self.difference("mask_request_metadata", row)
        self.lane_pending[lane] = dict(row, owner=origin["owner"])

    def handshake(self, row):
        lane = row["lane"]
        push = self.lane_pending.pop(lane, None)
        if push is None or any(row[k] != push[k] for k in ("serial", "id", "start", "count", "eew", "vs", "scale")):
            self.fail("Request handshake lacks its complete matching push", row)
            return
        self.handshakes[lane] += 1
        for offset in range(push["count"]):
            self.vrf_requests[lane].append({"owner": push["owner"], "requested_word": push["start"] + offset, "phase": push["phase"]})

    def vrf_read(self, row):
        lane = row["lane"]
        if not self.vrf_requests[lane]:
            if self.outside(row):
                self.outside_window_events[row["event"]] += 1
                return
            self.fail("Physical VRF read has no accepted MaskM word request", row)
            return
        token = self.vrf_requests[lane].popleft()
        word = row["row"] * 8 + row["bank"]
        if word != token["requested_word"]:
            self.difference("physical_vrf_address", row, lane=lane, owner=token["owner"], observed=word, requested=token["requested_word"])
        token.update(word=word, read_line=row["line"])
        if word // 16 not in range(5):
            self.difference("uninitialized_register_read", row, word=word, lane=lane)
        self.vrf_returns[lane].append(token)

    def vrf_return(self, row):
        lane = row["lane"]
        if not self.vrf_returns[lane]:
            if self.outside(row):
                self.outside_window_events[row["event"]] += 1
                return
            self.fail("Actual VRF return has no observed physical read", row)
            return
        token = self.vrf_returns[lane].popleft()
        token.update(data_bits=row["data_bits"].lower(), return_line=row["line"])
        four_state(token["data_bits"], 64)
        self.vrf_data[lane].append(token)

    def emit(self, row):
        origin = self.origin(row)
        if origin is None:
            return
        owner, phase = origin["owner"], origin["phase"]
        index = self.emits[owner]
        self.emits[owner] += 1
        operand, operand_known = four_state(row["operand_bits"], 128)
        strobes, known = four_state(row["mask_bits"], 16)
        valid, valid_known = four_state(row["valid_bits"], 2)
        ready, ready_known = four_state(row["operand_ready_bits"], 2)
        if valid_known != 3 or ready_known != 3:
            self.fail("Unknown operand ready or emitted valid lanes", row)
        for lane in (0, 1):
            if not self.vrf_data[lane]:
                self.fail("Actual sampled operand has no observed physical VRF source", row)
                continue
            token = self.vrf_data[lane][0]
            lane_value = (operand >> (lane * 64)) & ((1 << 64) - 1)
            lane_known = (operand_known >> (lane * 64)) & ((1 << 64) - 1)
            if token["owner"] != owner:
                self.difference("stale_vrf_operand_owner", row, lane=lane, produced_owner=token["owner"], expected_owner=owner)
            source_value, source_known = four_state(token["data_bits"], 64)
            if lane_known != source_known or (lane_value ^ source_value) & source_known:
                self.fail("Actual sampled operand differs from the oldest real VRF return", row)
            if ready & (1 << lane):
                self.vrf_data[lane].popleft()
                self.operand_pops[lane] += 1
        bitmask = sum(255 << (lane * 8) for lane in (0, 1) if valid & (1 << lane))
        rendered, render_known = sampled_strobe(operand, operand_known, row["pointer"], 8 << row["sew"], row["mask_eew"])
        if ((known ^ render_known) & bitmask) or ((strobes ^ rendered) & render_known & bitmask):
            self.fail("Actual emitted strobes do not follow the sampled operand and physical layout", row)
        packets = origin["model"]["packets"]
        if index >= len(packets):
            self.difference("surplus_mask_payload", row, owner=owner, payload=index)
        else:
            first = packets[index]
            wanted = architectural_strobe(self.nf, phase, first, self.sew)
            wanted_valid = (1 << min(origin["vl"] - first, 2)) - 1
            if valid != wanted_valid or known & bitmask != bitmask or ((strobes ^ wanted) & bitmask):
                self.difference("architectural_emitted_mask", row, owner=owner, payload=index, got=hex(strobes), known=hex(known), want=hex(wanted), valid=valid, wanted_valid=wanted_valid)
            if (row["read"], row["pointer"]) != (origin["vl"] - first, first % 128):
                self.difference("payload_interval_progression", row, owner=owner, got=[row["read"], row["pointer"]], want=[origin["vl"] - first, first % 128])
        slot = row["slot"]
        if slot not in (0, 1) or slot in self.slots:
            self.fail("EMIT overwrote an occupied physical mask slot", row)
            return
        self.slots[slot] = {"owner": owner, "phase": phase, "mask_bits": row["mask_bits"].lower(), "valid": valid, "delivered": Counter()}
        self.order.append(slot)

    def fill(self, row):
        if not self.order:
            self.fail("Actual consumer cut accepted data without an emitted mask slot", row)
            return
        source = self.slots[self.order[0]]
        lane = row["lane"]
        target = self.origin(row)
        if target is not None and source["owner"] != target["owner"]:
            self.difference("stale_mask_payload_at_cut_fill", row, lane=lane, produced_owner=source["owner"], target_owner=target["owner"])
        if (row["load"], row["store"]) != ((1, 0) if self.family == 1 else (0, 1)):
            self.fail("Cut fill lacks the unique configured memory direction", row)
        if not source["valid"] & (1 << lane):
            self.fail("Cut accepted an invalid emitted lane", row)
        expected_bits = source["mask_bits"][8 * (1 - lane):8 * (2 - lane)]
        if row["byte_bits"].lower() != expected_bits:
            self.fail("Actual cut byte differs from its physical emitted slot", row)
        direction = "load" if row["load"] else "store"
        queue = self.cuts[(direction, lane)]
        queue.append({"owner": source["owner"], "phase": source["phase"], "bits": expected_bits, "slot": self.order[0]})
        source["delivered"][lane] += 1
        self.fills[(direction, lane)] += 1
        if len(queue) > 2:
            self.fail("Actual consumer cut exceeds its two physical entries", row)

    def prefetch(self, row):
        slot = row["slot"]
        if not self.order or self.order[0] != slot or slot not in self.slots:
            self.fail("Actual prefetch does not retire the oldest physical mask slot", row)
            return
        source = self.slots.pop(slot)
        self.order.popleft()
        value, known = four_state(source["mask_bits"], 16)
        logged_mask = row["mask"].lower().lstrip("0") or "0"
        if known == 65535 and logged_mask != format(value, "x"):
            self.fail("Prefetch data differs from the emitted physical slot", row)
        if int(row["valid"], 16) != source["valid"]:
            self.fail("Prefetch lane-valid field differs from the emitted slot", row)
        for lane in (0, 1):
            wanted = 1 if source["valid"] & (1 << lane) else 0
            if source["delivered"][lane] != wanted:
                self.fail("Prefetch did not transfer each valid physical lane exactly once", row)

    def consume(self, row):
        origin = self.origin(row)
        if origin is None:
            return
        owner = origin["owner"]
        if (row["load"], row["store"]) != ((1, 0) if self.family == 1 else (0, 1)):
            self.fail("Consume lacks the unique configured memory direction", row)
        ordinal = self.consumes[owner]
        self.consumes[owner] += 1
        direction = "load" if row["load"] else "store"
        valid, known = four_state(row["valid_bits"], 2)
        mask, mask_known = four_state(row["mask_bits"], 16)
        if known != 3 or not valid:
            self.fail("Actual consume has unknown or empty valid lanes", row)
        for lane in (0, 1):
            if not valid & (1 << lane):
                continue
            queue = self.cuts[(direction, lane)]
            if not queue:
                self.fail("Actual consume has no prior per-lane cut fill", row)
                continue
            token = queue.popleft()
            bits = row["mask_bits"][8 * (1 - lane):8 * (2 - lane)].lower()
            if token["bits"] != bits:
                self.fail("Actual consumer byte differs from its oldest filled lane byte", row)
            if token["owner"] != owner:
                self.difference("stale_mask_payload_owner", row, lane=lane, produced_owner=token["owner"], consumer_owner=owner, producer_phase=token["phase"], consumer_phase=origin["phase"])
        packets = origin["model"]["packets"]
        if ordinal >= len(packets):
            self.difference("surplus_consumer_payload", row, owner=owner)
            return
        first = packets[ordinal]
        wanted = architectural_strobe(self.nf, origin["phase"], first, self.sew)
        wanted_valid = (1 << min(origin["vl"] - first, 2)) - 1
        bitmask = sum(255 << (lane * 8) for lane in (0, 1) if valid & (1 << lane))
        if valid != wanted_valid or mask_known & bitmask != bitmask or (mask ^ wanted) & bitmask:
            self.difference("architectural_consumed_mask", row, owner=owner, payload=ordinal, got=hex(mask), want=hex(wanted), known=hex(mask_known), valid=valid, wanted_valid=wanted_valid)

    def replay(self, rows):
        cycles = defaultdict(list)
        for row in rows:
            if "cycle" in row:
                cycles[row["cycle"]].append(row)
        for cycle in sorted(cycles):
            group = cycles[cycle]
            if not self.transition_states and any(r["event"] == "V01_MASK_UNIT_ACCEPT" and r["phase"] == 1 for r in group):
                self.transition_states.append(self.stage_state(1, cycle))
            # All values sample q-state before this edge. Cross-always display
            # ordering is not a hardware ordering. Retire before replacement.
            for event, function in (("V01_MASK_LANE_HANDSHAKE", self.handshake),
                                    ("V01_MASK_LANE_PUSH", self.lane_push),
                                    ("V01_MASK_VRF_READ", self.vrf_read),
                                    ("V01_MASK_VRF_RETURN", self.vrf_return),
                                    ("V01_MASK_UNIT_CONSUME", self.consume),
                                    ("V01_MASK_UNIT_CUT_FILL", self.fill),
                                    ("V01_MASK_UNIT_PREFETCH", self.prefetch),
                                    ("V01_MASK_UNIT_EMIT", self.emit)):
                for row in group:
                    if row["event"] == event:
                        function(row)
            for row in group:
                if row["event"] == "V01_MASK_UNIT_DONE":
                    origin = self.origin(row, False)
                    if origin is not None:
                        if origin["owner"] in self.done:
                            self.fail("Duplicate completion for one actual owner", row)
                        self.done.add(origin["owner"])
                        wanted = len(origin["model"]["packets"])
                        if self.emits[origin["owner"]] != wanted:
                            self.difference("completed_payload_count", row, owner=origin["owner"], observed=self.emits[origin["owner"]], wanted=wanted)

    def finish(self, rows):
        counts = Counter(r["event"] for r in rows)
        lane_ready = [r for r in rows if r["event"] == "V01_MASK_LANE_READY"]
        unit_ready = [r for r in rows if r["event"] == "V01_MASK_UNIT_READY"]
        if len(lane_ready) != 2 or {r["lane"] for r in lane_ready} != {0, 1} or any((r["lanes"], r["vlenbits"]) != (2, 12) for r in lane_ready):
            self.fail("Missing exact two-lane configuration observations")
        if len(unit_ready) != 1 or (unit_ready[0]["lanes"], unit_ready[0]["depth"], unit_ready[0]["vlenbits"]) != (2, 1, 12):
            self.fail("Missing actual MASKU depth-one configuration")
        if not counts["V01_MASK_RESET"]:
            self.fail("Observer lacks reset-disturbance records")
        begins = [r for r in rows if r["event"] == PHASE_BEGIN]
        ends = [r for r in rows if r["event"] == PHASE_DONE]
        expected_phases = list(range(3))
        if [r["phase"] for r in begins] != expected_phases[:len(begins)] or len(begins) > 3:
            self.fail("Phase begin sequence is not an exact prefix of 0,1,2")
        if [r["phase"] for r in ends] != expected_phases[:len(ends)] or len(ends) > 3:
            self.fail("Phase completion sequence is not an exact prefix of 0,1,2")
        checks_per_phase = 6 + 2 * self.nf * 256 + 512 + 2 * 512 + 2 * 256 + 14
        selected = 7 if self.nf == 5 else 1
        for row in begins:
            wanted = (selected + 8 * row["phase"], self.family, self.nf, int(row["phase"] < 2))
            if tuple(row[k] for k in ("id", "family", "nf", "masked")) != wanted:
                self.fail("Phase begin does not match the exact public input", row)
        for row in ends:
            wanted = (selected + 8 * row["phase"], 2 + (row["phase"] + 1) * checks_per_phase, row["phase"] + 1)
            if tuple(row[k] for k in ("id", "checks", "completed")) != wanted:
                self.fail("Phase completion counters differ from the actual three-phase input", row)
            if row["errors"]:
                self.difference("software_phase_errors", row, errors=row["errors"], phase=row["phase"])
        for lane in (0, 1):
            finals = [r for r in rows if r["event"] == "V01_MASK_LANE_SUMMARY" and r["lane"] == lane]
            if not finals and self.incomplete:
                finals = [r for r in rows if r["event"] == "V01_MASK_LANE_SNAPSHOT" and r["lane"] == lane]
            if len(finals) != 1:
                self.fail("Missing unique actual lane final or incomplete-run snapshot")
            else:
                final_lane = finals[0]
                wanted = (sum(v for (l, _), v in self.pushes.items() if l == lane), self.handshakes[lane], 0, int(lane in self.lane_pending))
                if tuple(final_lane[k] for k in ("pushes", "handshakes", "hold_errors", "pending")) != wanted:
                    self.fail("Lane final differs from the actual request event stream", final_lane)
        for row in rows:
            if any(part in row["event"] for part in ("HOLD_ERROR", "OVERWRITE", "LANE_FLUSH")):
                self.fail("Actual request hold/overwrite/flush failure", row)
            if begins and row["line"] >= begins[0]["line"] and (len(ends) < 3 or row["line"] <= ends[-1]["line"]):
                if row["event"] in ("V01_MASK_RESET", "V01_MASK_FLUSH", "V01_MASK_OTHER_ACCEPT"):
                    self.difference("phase_disturbance", row, event=row["event"])
        summary = [r for r in rows if r["event"] == "V01_MASK_UNIT_SUMMARY"]
        snapshots = [r for r in rows if r["event"] == "V01_MASK_UNIT_SNAPSHOT"]
        final = summary[0] if len(summary) == 1 else snapshots[0] if self.incomplete and len(snapshots) == 1 else None
        if final is None:
            self.fail("Missing unique actual final summary or incomplete-run snapshot")
        else:
            actual_counts = [len(self.owners), sum(self.emits.values()), len(self.done)]
            if [final[k] for k in ("accepted", "emitted", "completed")] != actual_counts:
                self.fail("MASKU summary does not match the complete observed event list", final)
            if final["queue"] != len(self.slots):
                self.fail("Final physical slot occupancy differs from replay", final)
            for direction, prefix in (("load", "ldu"), ("store", "stu")):
                valid, vk = four_state(final[prefix + "_cut_valid_bits"], 2)
                ready, rk = four_state(final[prefix + "_cut_ready_bits"], 2)
                if vk != 3 or rk != 3:
                    self.fail("Unknown final actual consumer cut state", final)
                    continue
                for lane in (0, 1):
                    state = ((valid >> lane) & 1, (ready >> lane) & 1)
                    occupancy = {(0, 1): 0, (1, 1): 1, (1, 0): 2}.get(state)
                    if occupancy != len(self.cuts[(direction, lane)]):
                        self.fail("Actual cut occupancy differs from per-lane FIFO conservation", final)
        complete = len(ends) == 3 and len(summary) == 1 and not self.incomplete
        if complete:
            wanted = 2 * self.vl * self.nf
            if len(self.owners) != wanted or len(self.done) != wanted:
                self.difference("incomplete_masked_owners", final or rows[-1], accepted=len(self.owners), completed=len(self.done), wanted=wanted)
            for owner in self.owners:
                count = len(owner["model"]["packets"])
                if self.consumes[owner["owner"]] != count:
                    self.difference("consumer_payload_count", owner, owner=owner["owner"], observed=self.consumes[owner["owner"]], wanted=count)
            if self.slots or any(self.cuts.values()) or self.lane_pending or any(self.vrf_requests.values()) or any(self.vrf_returns.values()) or any(self.vrf_data.values()):
                self.difference("final_not_drained", final or rows[-1])
            if final and any(final[k] for k in ("queue", "issue_queue", "commit_queue", "ldu_valid", "ldu_remaining", "stu_valid", "stu_remaining")):
                self.difference("actual_final_not_drained", final)
        elif not self.incomplete:
            self.difference("missing_complete_three_phase_run", rows[-1] if rows else {"line": 0})
        return {"assessment": "INVALID_EVIDENCE" if self.errors else "PASS" if complete and not self.difference_counts else "DIAGNOSTIC",
                "complete_three_phases": complete, "integrity_errors": self.errors,
                "semantic_difference_counts": dict(self.difference_counts), "semantic_differences": dict(self.differences),
                "actual_event_counts": dict(counts), "accepted_owners": len(self.owners),
                "unrelated_vrf_events_outside_phase_window": dict(self.outside_window_events),
                "completed_owners": len(self.done), "emitted_payloads": sum(self.emits.values()),
                "consumed_payloads": sum(self.consumes.values()), "transition_states": self.transition_states,
                "final_replayed_state": self.stage_state(None, final["cycle"] if final else None),
                "remaining_physical_vrf_words": {str(lane): len(q) for lane, q in self.vrf_data.items()},
                "scope": "Passive real-event review only. Separate native completion, assertions, and full-signature review remain required."}


def review(rows, nf, family, incomplete=False):
    checker = Review(nf, family, incomplete)
    checker.prepare(rows)
    checker.replay(rows)
    return checker.finish(rows)


def deadline_binding(rows, deadline_ns):
    snapshots = [r for r in rows if r["event"] in ("V01_MASK_LANE_SNAPSHOT", "V01_MASK_UNIT_SNAPSHOT")]
    if len(snapshots) != 3 or any(r["time_ns"] != deadline_ns for r in snapshots) or len({r["cycle"] for r in snapshots}) != 1:
        raise ValueError("Incomplete diagnosis requires two lane snapshots and one unit snapshot at the exact frozen deadline")
    units = [r for r in snapshots if r["event"] == "V01_MASK_UNIT_SNAPSHOT"]
    lanes = [r for r in snapshots if r["event"] == "V01_MASK_LANE_SNAPSHOT"]
    if len(units) != 1 or {r["lane"] for r in lanes} != {0, 1}:
        raise ValueError("Incomplete diagnosis lacks the exact physical unit/lane snapshot set")
    finals = [r for r in rows if r["event"] in ("V01_MASK_LANE_SUMMARY", "V01_MASK_UNIT_SUMMARY")]
    if finals:
        if len(finals) != 3:
            raise ValueError("Incomplete diagnosis contains partial or duplicate actual final summaries")
        for snapshot in snapshots:
            matches = [r for r in finals if r["event"] == snapshot["event"].replace("SNAPSHOT", "SUMMARY") and r.get("lane") == snapshot.get("lane")]
            fields = lambda r: {k: v for k, v in r.items() if k not in ("event", "line", "phase")}
            if len(matches) != 1 or fields(matches[0]) != fields(snapshot):
                raise ValueError("Actual final state differs from its same-deadline snapshot")
    return {"deadline_ns": deadline_ns, "snapshot_lines": [r["line"] for r in snapshots],
            "actual_final_lines": [r["line"] for r in finals],
            "summary_source": "actual_final_and_matching_snapshot" if finals else "actual_deadline_snapshot_only"}


def fixture(nf=5, family=1):
    """Artificial complete stream. These rows are not simulation evidence."""
    rows = []
    def add(event, phase=None, **fields):
        rows.append(dict(event=event, line=len(rows) + 1, phase=phase, **fields))
    add("V01_MASK_RESET", cycle=0, level="1", time_ns=15)
    add("V01_MASK_UNIT_READY", lanes=2, depth=1, vlenbits=12)
    for lane in (0, 1):
        add("V01_MASK_LANE_READY", lane=lane, lanes=2, vlenbits=12, reqbits=128)
    sew, vl, selected = (64, 3, 7) if nf == 5 else (8, 129, 1)
    vfu = 4 if family == 1 else 5
    count = 0
    cycle = 20
    check_count = 6 + 2 * nf * 256 + 512 + 2 * 512 + 2 * 256 + 14
    for phase in range(3):
        add(PHASE_BEGIN, phase, id=selected + 8 * phase, family=family, nf=nf, masked=int(phase < 2))
        if phase < 2:
            for element in range(vl):
                for field in range(nf):
                    count += 1
                    model = interval(sew, element, element + 1)
                    meta = dict(id=count % 8, vd=8 + field, sew={8: 0, 64: 3}[sew], vl=element + 1, vstart=element)
                    add("V01_MASK_UNIT_ACCEPT", phase, cycle=cycle, serial=count, vfu=vfu, mask_eew=0,
                        read_next=model["remaining"], commit_next=model["remaining"], pointer_next=model["pointer"],
                        issue_queue=0, commit_queue=0, **meta)
                    for lane in (0, 1):
                        request = dict(lane=lane, serial=count, id=meta["id"], start=model["rows"][0], count=len(model["rows"]), eew=3, vs=0, scale=0)
                        add("V01_MASK_LANE_PUSH", phase, cycle=cycle, pevalid=1, vd=meta["vd"], vfu=vfu,
                            sew=meta["sew"], vl=meta["vl"], vstart=meta["vstart"], **request)
                        add("V01_MASK_LANE_HANDSHAKE", phase, cycle=cycle + 1, **request)
                    word = model["rows"][0]
                    operand = 0
                    for lane in (0, 1):
                        data = physical_vrf_word(nf, phase, word, lane)
                        operand |= data << (lane * 64)
                        add("V01_MASK_VRF_READ", phase, cycle=cycle + 2, lane=lane, row=word // 8, bank=word % 8)
                        add("V01_MASK_VRF_RETURN", phase, cycle=cycle + 3, lane=lane, data_bits=format(data, "064b"))
                    mask = architectural_strobe(nf, phase, model["packets"][0], sew)
                    valid = (1 << min(model["remaining"], 2)) - 1
                    slot = (count - 1) % 2
                    add("V01_MASK_UNIT_EMIT", phase, cycle=cycle + 4, serial=count, mask_eew=0,
                        read=model["remaining"], pointer=model["pointer"], operand_bits=format(operand, "0128b"),
                        operand_ready_bits="11", mask_bits=format(mask, "016b"), valid_bits=format(valid, "02b"), slot=slot, **meta)
                    for lane in (0, 1):
                        if valid & (1 << lane):
                            add("V01_MASK_UNIT_CUT_FILL", phase, cycle=cycle + 5, load=int(family == 1), store=int(family == 2),
                                lane=lane, serial=count, current_masku_id=meta["id"], byte_bits=format((mask >> (lane * 8)) & 255, "08b"), **meta)
                    add("V01_MASK_UNIT_PREFETCH", phase, cycle=cycle + 5, serial=count, id=meta["id"], valid=format(valid, "x"),
                        mask=format(mask, "04x"), load=int(family == 1), store=int(family == 2), queue=1, slot=slot)
                    add("V01_MASK_UNIT_CONSUME", phase, cycle=cycle + 6, load=int(family == 1), store=int(family == 2),
                        valid_bits=format(valid, "02b"), mask_bits=format(mask, "016b"), **meta)
                    add("V01_MASK_UNIT_DONE", phase, cycle=cycle + 7, serial=count, id=meta["id"], vd=meta["vd"], vl=meta["vl"], vstart=meta["vstart"], queue=0)
                    cycle += 10
        add(PHASE_DONE, phase, id=selected + 8 * phase, checks=2 + (phase + 1) * check_count, errors=0, completed=phase + 1)
    for lane in (0, 1):
        add("V01_MASK_LANE_SUMMARY", 2, cycle=cycle, lane=lane, time_ns=cycle, pushes=count,
            handshakes=count, stalls=0, hold_errors=0, pending=0)
    add("V01_MASK_UNIT_SUMMARY", 2, cycle=cycle, time_ns=cycle, accepted=count, emitted=count,
        completed=count, queue=0, issue_queue=0, commit_queue=0, read=0, commit=0, pointer=0,
        operand_valid_bits="00", ldu_valid=0, ldu_remaining=0, ldu_cut_valid_bits="00", ldu_cut_ready_bits="11",
        stu_valid=0, stu_remaining=0, stu_cut_valid_bits="00", stu_cut_ready_bits="11")
    return rows


def self_test():
    import copy
    results = []
    for nf in (5, 2):
        for family in (1, 2):
            rows = fixture(nf, family)
            positive = review(rows, nf, family)["assessment"] == "PASS"
            controls = {}
            mutations = {
                "wrong_slot": ("V01_MASK_UNIT_PREFETCH", "slot", 99),
                "wrong_cut_byte": ("V01_MASK_UNIT_CUT_FILL", "byte_bits", "xxxxxxxx"),
                "wrong_return_data": ("V01_MASK_VRF_RETURN", "data_bits", "x" * 64),
                "wrong_physical_word": ("V01_MASK_VRF_READ", "bank", 7),
                "phase_interference": ("V01_MASK_UNIT_DONE", "event", "V01_MASK_OTHER_ACCEPT"),
                "unknown_operand_ready": ("V01_MASK_UNIT_EMIT", "operand_ready_bits", "xx"),
            }
            for name, (event, key, value) in mutations.items():
                changed = copy.deepcopy(rows)
                next(r for r in changed if r["event"] == event)[key] = value
                controls[name] = review(changed, nf, family)["assessment"] != "PASS"
            changed = [r for r in rows if r["event"] != "V01_MASK_RESET"]
            controls["missing_reset_observation"] = review(changed, nf, family)["assessment"] == "INVALID_EVIDENCE"
            changed = copy.deepcopy(rows)
            for row in changed:
                if row["event"] in ("V01_MASK_UNIT_CUT_FILL", "V01_MASK_UNIT_EMIT", "V01_MASK_UNIT_PREFETCH"):
                    row["current_masku_id"] = 999
                    row["serial"] = 999
                    if row["event"] == "V01_MASK_UNIT_PREFETCH":
                        row["id"] = 999
            ignored_current_id = review(changed, nf, family)["assessment"] == "PASS"
            lines = []
            for row in rows:
                fields = {k: v for k, v in row.items() if k not in ("event", "line", "phase")}
                if row["event"] in (PHASE_BEGIN, PHASE_DONE):
                    fields["phase"] = row["phase"]
                lines.append(row["event"] + " " + " ".join("%s=%s" % item for item in fields.items()))
            class MemoryLog:
                def read_text(self, **unused):
                    return "\n".join(lines)
            parser_roundtrip = review(parse_log(MemoryLog()), nf, family)["assessment"] == "PASS"
            results.append(dict(nf=nf, family=family, positive_fixture=positive,
                                rejected_mutations=controls, current_id_is_not_owner=ignored_current_id,
                                parser_roundtrip=parser_roundtrip))
    passed = all(r["positive_fixture"] and r["current_id_is_not_owner"] and r["parser_roundtrip"] and all(r["rejected_mutations"].values()) for r in results)
    return {"assessment": "PASS" if passed else "FAIL", "kind": "artificial_passive_event_fixtures",
            "rtl_execution": False, "actual_run_data": False, "results": results}


def negative_controls(rows, nf, family):
    """Change real event data in memory. Keep the original log unchanged."""
    import copy
    if review(rows, nf, family)["assessment"] != "PASS":
        raise ValueError("Real event controls require a complete passing event stream")
    rejected = {}
    for name, event, key, value in (
            ("wrong_physical_word", "V01_MASK_VRF_READ", "bank", 7),
            ("wrong_mask_cut_byte", "V01_MASK_UNIT_CUT_FILL", "byte_bits", "xxxxxxxx"),
            ("wrong_consumed_byte", "V01_MASK_UNIT_CONSUME", "mask_bits", "x" * 16),
            ("wrong_prefetched_slot", "V01_MASK_UNIT_PREFETCH", "slot", 99),
            ("wrong_returned_operand", "V01_MASK_VRF_RETURN", "data_bits", "x" * 64)):
        changed = copy.deepcopy(rows)
        target = next(r for r in changed if r["event"] == event)
        if target[key] == value:
            raise ValueError("An event negative control did not change its actual input")
        target[key] = value
        rejected[name] = review(changed, nf, family)["assessment"] != "PASS"
    changed = copy.deepcopy(rows)
    emissions = [r for r in changed if r["event"] == "V01_MASK_UNIT_EMIT"]
    if len(emissions) < 2:
        raise ValueError("Owner mutation requires two actual emitted owners")
    for key in META:
        emissions[1][key] = emissions[0][key]
    rejected["payload_owner_reassigned_without_data_change"] = review(changed, nf, family)["assessment"] != "PASS"
    return {"assessment": "PASS" if all(rejected.values()) else "FAIL", "rejected": rejected,
            "scope": "In-memory mutations of the actual recorded event stream. No new RTL run."}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", type=Path)
    parser.add_argument("--ara-pkg", type=Path)
    parser.add_argument("--nf", type=int, choices=(2, 5))
    parser.add_argument("--family", type=int, choices=(1, 2))
    parser.add_argument("--variant", help="A label only. It does not choose a legacy error profile.")
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--negative-controls", action="store_true", help="Mutate actual passing events in memory and require rejection")
    parser.add_argument("--incomplete", action="store_true")
    parser.add_argument("--deadline-ns", type=int, help="Frozen positive HDL deadline, required for an incomplete run")
    parser.add_argument("--diagnostic", action="store_true", help="Permit recorded semantic differences, never missing evidence")
    args = parser.parse_args()
    if args.self_test:
        result = self_test()
        print(json.dumps(result, indent=2))
        return 0 if result["assessment"] == "PASS" else 1
    if any(getattr(args, key) is None for key in ("log", "ara_pkg", "nf", "family", "variant")):
        parser.error("A real review requires --log, --ara-pkg, --nf, --family, and --variant")
    if args.incomplete and (args.deadline_ns is None or args.deadline_ns <= 0):
        parser.error("An incomplete run requires a positive frozen --deadline-ns")
    try:
        binding = table_binding(args.ara_pkg)
        rows = parse_log(args.log)
        result = review(rows, args.nf, args.family, args.incomplete)
        if args.incomplete:
            result["deadline_binding"] = deadline_binding(rows, args.deadline_ns)
        if args.negative_controls:
            result["negative_controls"] = negative_controls(rows, args.nf, args.family)
            if result["negative_controls"]["assessment"] != "PASS":
                result["assessment"] = "INVALID_EVIDENCE"
        result.update(ara_pkg_binding=binding, variant=args.variant)
    except (ValueError, KeyError, IndexError) as exc:
        result = {"assessment": "INVALID_EVIDENCE", "error": str(exc)}
    result["log_sha256"] = hashlib.sha256(args.log.read_bytes()).hexdigest()
    result["checker_sha256"] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    print(json.dumps(result, indent=2))
    return 0 if result["assessment"] == "PASS" or args.diagnostic and result["assessment"] == "DIAGNOSTIC" else 1


if __name__ == "__main__":
    raise SystemExit(main())
