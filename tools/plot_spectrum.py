#!/usr/bin/env python3
"""Plota o CSV gerado por `lavoe-salsa --dump`.

Uso:
    lavoe-salsa --dump | python3 tools/plot_spectrum.py
    python3 tools/plot_spectrum.py dump.csv -o spec.png
"""
import argparse
import sys

import matplotlib.pyplot as plt
import numpy as np


def read_rows(stream) -> np.ndarray:
    rows = []
    for line in stream:
        line = line.strip()
        if not line:
            continue
        try:
            rows.append([float(v) for v in line.split(",")])
        except ValueError:
            continue
    if not rows:
        sys.exit("nenhuma linha CSV valida encontrada")
    n = max(len(r) for r in rows)
    arr = np.zeros((len(rows), n), dtype=float)
    for i, r in enumerate(rows):
        arr[i, : len(r)] = r
    return arr


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("csv", nargs="?", default="-",
                    help="arquivo CSV do --dump (padrao: stdin)")
    ap.add_argument("-o", "--output", help="salva em arquivo em vez de exibir")
    ap.add_argument("--max-linhas", type=int, default=600,
                    help="maximo de frames exibidos (amostra uniforme)")
    args = ap.parse_args()

    if args.csv == "-":
        arr = read_rows(sys.stdin)
    else:
        with open(args.csv, "r", encoding="utf-8") as fh:
            arr = read_rows(fh)

    if arr.shape[0] > args.max_linhas:
        idx = np.linspace(0, arr.shape[0] - 1, args.max_linhas).astype(int)
        arr = arr[idx]

    fig, ax = plt.subplots(figsize=(14, 6))
    im = ax.imshow(arr.T, aspect="auto", origin="lower", cmap="inferno",
                   vmin=0.0, vmax=1.0, extent=(0, arr.shape[0], 0, arr.shape[1]))
    ax.set_xlabel("frame")
    ax.set_ylabel("barra")
    ax.set_title("Lavoe Salsa — espectro (lavoe-salsa --dump)")
    fig.colorbar(im, ax=ax, label="altura (0..1)")
    fig.tight_layout()
    if args.output:
        fig.savefig(args.output, dpi=110)
        print(f"salvo em {args.output}")
    else:
        plt.show()


if __name__ == "__main__":
    main()
