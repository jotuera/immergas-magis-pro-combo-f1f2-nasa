#!/usr/bin/env python3
"""Correlate undecoded Samsung NASA PDUs with known reference signals.

Feed it an ESPHome log captured with the **Discovery** switch on (package f1f2_diagnostics). Each change
line then carries a context snapshot, e.g.:

    [23:41:02][I][f1f2_sniff:201]: 10.00.00 840F: 0 -> 1 | comp=30 drv=2 mode=4 defr=0 night=286 3w=0 tout=50

For every PDU this script reports which reference signal its value tracks best (Pearson |r|), so you can guess
what an unknown PDU means (e.g. "0x840F tracks comp (compressor running)"). Pure stdlib, no dependencies.

Usage:
    python sniffer_correlate.py <logfile>              # all PDUs, best-correlated reference each
    python sniffer_correlate.py <logfile> --pdu 840F   # focus one PDU, full breakdown
    python sniffer_correlate.py <logfile> --min 8      # ignore PDUs with fewer than N samples (default 5)

References in the context (raw NASA values):
    comp  0x8238 compressor Hz     drv 0x8001 outdoor driving mode   mode 0x4001 indoor operation mode
    defr  0x402E defrost flag      night 0x428D night-function req    3w   0x4067 three-way valve (0=heat,1=DHW)
    tout  0x8204 outdoor temp (x10)
A reference value of -1 means "not seen yet" and that sample is skipped for that reference.
"""
import argparse
import math
import re
import sys
from collections import defaultdict

# a discovery line: "<src> <PDU>: <old> -> <new> | <ctx>"  or  "<src> <PDU> = <val> (new) | <ctx>"
LINE = re.compile(
    r"(?P<src>[0-9A-Fa-f]{2}\.[0-9A-Fa-f]{2}\.[0-9A-Fa-f]{2})\s+(?P<pdu>[0-9A-Fa-f]{4})"
    r"(?::\s*\d+\s*->\s*(?P<new>-?\d+)|\s*=\s*(?P<newnew>-?\d+)\s*\(new\))"
    r"\s*\|\s*(?P<ctx>.+)$"
)
REFS = ["comp", "drv", "mode", "defr", "night", "3w", "tout"]
# how each reference is turned into a feature for the correlation: raw value, or a 0/1 derived flag
DERIVED = {
    "comp_on": ("comp", lambda v: 1.0 if v > 0 else 0.0),
    "defrost": ("defr", lambda v: 1.0 if v != 0 else 0.0),
    "night": ("night", lambda v: 1.0 if v != 0 else 0.0),
    "dhw": ("3w", lambda v: 1.0 if v == 1 else 0.0),
    "comp_hz": ("comp", lambda v: float(v)),
    "drv": ("drv", lambda v: float(v)),
    "mode": ("mode", lambda v: float(v)),
    "tout": ("tout", lambda v: float(v) / 10.0),
}


def pearson(xs, ys):
    n = len(xs)
    if n < 3:
        return None
    mx, my = sum(xs) / n, sum(ys) / n
    sxx = sum((x - mx) ** 2 for x in xs)
    syy = sum((y - my) ** 2 for y in ys)
    if sxx == 0 or syy == 0:
        return None  # one side constant -> undefined
    sxy = sum((x - mx) * (y - my) for x, y in zip(xs, ys))
    return sxy / math.sqrt(sxx * syy)


def parse(path):
    samples = defaultdict(list)  # (src, pdu) -> list of (value, {ref: int})
    for raw in open(path, encoding="utf-8", errors="replace"):
        m = LINE.search(raw)
        if not m:
            continue
        val = m.group("new") or m.group("newnew")
        ctx = {}
        for kv in m.group("ctx").split():
            if "=" in kv:
                k, _, v = kv.partition("=")
                if k in REFS:
                    try:
                        ctx[k] = int(v)
                    except ValueError:
                        pass
        if len(ctx) == len(REFS):
            samples[(m.group("src").upper(), m.group("pdu").upper())].append((int(val), ctx))
    return samples


def analyse_pdu(values_ctx):
    vals = [v for v, _ in values_ctx]
    out = []
    for feat, (ref, fn) in DERIVED.items():
        xs, ys = [], []
        for v, ctx in values_ctx:
            r = ctx.get(ref, -1)
            if r == -1:
                continue
            xs.append(float(v))
            ys.append(fn(r))
        r = pearson(xs, ys)
        if r is not None:
            out.append((abs(r), r, feat, len(xs)))
    out.sort(reverse=True)
    return vals, out


def main():
    ap = argparse.ArgumentParser(description="Correlate NASA PDUs with reference signals from a Discovery log.")
    ap.add_argument("logfile")
    ap.add_argument("--pdu", help="focus a single PDU (hex, e.g. 840F), full breakdown")
    ap.add_argument("--min", type=int, default=5, help="skip PDUs with fewer than N samples (default 5)")
    a = ap.parse_args()

    samples = parse(a.logfile)
    if not samples:
        print("No Discovery lines found. Turn on the 'Discovery' switch and capture the f1f2_sniff log.")
        sys.exit(1)

    if a.pdu:
        want = a.pdu.upper().replace("0X", "")
        rows = [(k, v) for k, v in samples.items() if k[1] == want]
        if not rows:
            print(f"PDU {want} not seen in the log.")
            sys.exit(1)
        for (src, pdu), vc in rows:
            vals, out = analyse_pdu(vc)
            print(f"\n0x{pdu} ({src}) - {len(vc)} changes, distinct values: {sorted(set(vals))}")
            for absr, r, feat, n in out:
                print(f"    {feat:8s} r={r:+.2f}  (n={n})")
        return

    print(f"{'PDU':>7} {'src':>9} {'n':>4}  {'values':<22} best-correlated reference")
    print("-" * 78)
    results = []
    for (src, pdu), vc in samples.items():
        if len(vc) < a.min:
            continue
        vals, out = analyse_pdu(vc)
        best = out[0] if out else None
        results.append((best[0] if best else 0.0, pdu, src, len(vc), sorted(set(vals)), best))
    results.sort(reverse=True)
    for score, pdu, src, n, vals, best in results:
        vs = ",".join(str(x) for x in vals[:6]) + ("…" if len(vals) > 6 else "")
        tag = f"{best[2]} (r={best[1]:+.2f})" if best else "-"
        print(f"0x{pdu:>5} {src:>9} {n:>4}  {vs:<22} {tag}")
    print("\nHigh |r| with comp_on/defrost/night/dhw => a flag that tracks that state.")
    print("High |r| with comp_hz/tout => a value that scales with it. Use --pdu <hex> for the full breakdown.")


if __name__ == "__main__":
    main()
