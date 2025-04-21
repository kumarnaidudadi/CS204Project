#!/usr/bin/env python3
import re
import json
import sys
import argparse

def cast_value(v):
    """Convert "true"/"false" to bool, decimals to int (if all digits), else keep string."""
    vl = v.lower()
    if vl in ("true", "false"):
        return vl == "true"
    if re.fullmatch(r"-?\d+", v):
        return int(v, 0)
    return v

def split_kv(pair):
    """Split a key/value pair on the first '=' or ':', whichever comes first."""
    if '=' in pair:
        return pair.split('=', 1)
    if ':' in pair:
        return pair.split(':', 1)
    return None, None

def parse_pipeline_dump(lines):
    result = {
        "initial": {},
        "knobs": {},
        "cycles": [],
        "summary": {}
    }

    it = iter(lines)

    # --- initial settings ---
    for line in it:
        if m := re.match(r"Using default machine code file:\s*(\S+)", line):
            result["initial"]["machine_code_file"] = m.group(1)
        elif m := re.match(r"Set initial PC to:\s*(\S+)", line):
            result["initial"]["initial_pc"] = m.group(1)
        elif m := re.match(r"Parsing done\. Loaded\s*(\d+)\s*instructions\.", line):
            result["initial"]["instructions_loaded"] = int(m.group(1))
        elif line.startswith("--- Starting Pipelined Simulation"):
            break

    # --- knobs ---
    line = next(it)
    if m := re.match(r"Knobs:\s*(.*)", line):
        for kv in m.group(1).split(','):
            k, v = (x.strip() for x in kv.split('=',1))
            result["knobs"][k] = cast_value(v)

    # --- per‐cycle parsing ---
    current = None
    for line in it:
        if m := re.match(r"--- Cycle:\s*(\d+)\s*---", line):
            if current:
                result["cycles"].append(current)
            current = {"cycle": int(m.group(1)), "stages": [], "misc": []}
        elif re.match(r"\s*(IF/ID|ID/EX|EX/MEM|MEM/WB)", line):
            # split stage name and rest
            stage, rest = line.strip().split(' ', 1)
            fields = {}

            # if there's a [bracketed list], pull it out
            if b := re.match(r"\[(.?)\]:\s(.*)", rest):
                for part in b.group(1).split(','):
                    k, v = (p.strip() for p in part.split(':',1))
                    fields[k] = cast_value(v)
                rest = b.group(2)

            # now split everything else by commas
            for part in rest.split(','):
                part = part.strip()
                if not part:
                    continue
                k, v = split_kv(part)
                if k:
                    fields[k.strip()] = cast_value(v.strip())

            current["stages"].append({"stage": stage, "fields": fields})

        elif line.strip().startswith(("JUMP TARGET MISPREDICT", "MEM:", "WB:")):
            current["misc"].append(line.strip())

        elif line.startswith("--- Simulation Complete"):
            if current:
                result["cycles"].append(current)
            break

    # --- summary parsing ---
    for line in it:
        # total cycles
        if m := re.match(r"Total Clock Cycles:\s*(\d+)", line):
            result["summary"]["total_clock_cycles"] = int(m.group(1))

        # register file
        elif line.startswith("Register File State"):
            regs = {}
            buffer = ""
            # collect all non-empty lines
            for regline in it:
                if not regline.strip():
                    break
                buffer += regline.strip() + " "
            # find all x#: 0xVALUE pairs
            for m in re.finditer(r"(x\d+):\s*(0x[0-9A-Fa-f]+)", buffer):
                regs[m.group(1)] = m.group(2)
            result["summary"]["registers"] = regs

        # data memory
        elif line.startswith("Data Memory State"):
            mem = {}
            # skip header line
            next(it, None)
            for memline in it:
                if not memline.strip():
                    break
                addr, bytestr = memline.strip().split(':',1)
                mem[addr] = bytestr.strip()
            result["summary"]["data_memory"] = mem

    return result

def main():
    parser = argparse.ArgumentParser(
        description="Parse pipelined‐simulator dump into JSON"
    )
    parser.add_argument("infile", nargs='?', type=argparse.FileType('r'),
                        default=sys.stdin,
                        help="input dump file (default: stdin)")
    parser.add_argument("-o", "--outfile", type=argparse.FileType('w'),
                        default=sys.stdout,
                        help="output JSON file (default: stdout)")
    args = parser.parse_args()

    lines = args.infile.readlines()
    parsed = parse_pipeline_dump(lines)
    json.dump(parsed, args.outfile, indent=2)

if _name_ == "_main_":
    main()
