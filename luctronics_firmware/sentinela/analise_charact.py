#!/usr/bin/env python3
"""
Analise dos ensaios do node Sentinela.

  python analise_charact.py ensaio.csv
  python analise_charact.py ensaio.csv --mark 1     # analisa so o trecho marcado com 1

Responde duas perguntas:
  IR  -> a energia em 5-15 Hz (flicker de chama) separa fogo de lampada/sol?
  SOM -> os dois modulos tem ganho parecido o bastante para comparacao direcional?

Requer: numpy scipy pandas matplotlib
"""

import argparse
import sys

import numpy as np
import pandas as pd
from scipy.signal import butter, filtfilt, welch
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

FS = 200.0            # Hz, alvo do sketch
FLAME_BAND = (5, 15)  # Hz - flicker de chama
ALIAS_BAND = (60, 99) # Hz - onde 100/120 Hz rebatem ao amostrar a 200 Hz
IR_COLS = ["ir1", "ir2", "ir3"]
SND_COLS = ["som1", "som2"]


def load(path, mark=None):
    df = pd.read_csv(path, comment="#")
    df = df.dropna()
    if mark is not None:
        idx = df.index[df["mark"] == mark]
        if len(idx) == 0:
            sys.exit(f"nenhuma amostra com mark={mark}")
        df = df.loc[idx[0]:idx[-1]]
    t = df["t_us"].to_numpy(dtype=np.float64) / 1e6
    t -= t[0]
    return df, t


def resample(t, y, fs=FS):
    """Grade uniforme - o timestamp real corrige o jitter do timer."""
    grid = np.arange(0, t[-1], 1.0 / fs)
    return grid, np.interp(grid, t, y)


def band_rms(y, fs, lo, hi):
    b, a = butter(4, [lo / (fs / 2), hi / (fs / 2)], btype="band")
    return float(np.sqrt(np.mean(filtfilt(b, a, y - y.mean()) ** 2)))


def envelope(y, fs, win_ms=50):
    n = max(1, int(fs * win_ms / 1000))
    x = (y - y.mean()) ** 2
    k = np.ones(n) / n
    return np.sqrt(np.convolve(x, k, mode="same"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv")
    ap.add_argument("--mark", type=int, default=None)
    ap.add_argument("--out", default="analise")
    args = ap.parse_args()

    df, t = load(args.csv, args.mark)
    dt = np.diff(t)
    print(f"amostras={len(t)}  duracao={t[-1]:.1f}s")
    print(f"fs medida={1/np.median(dt):.1f} Hz  jitter p95={np.percentile(dt,95)*1e3:.2f} ms\n")

    lsb = 3.3 / 4095.0  # aproximado, so para dar escala em mV

    print("=== IR (flicker de chama) ===")
    print(f"{'canal':<6}{'DC(cont)':>10}{'5-15Hz(mV)':>13}{'60-99Hz(mV)':>13}{'alias?':>9}")
    ir_res = {}
    for c in IR_COLS:
        g, y = resample(t, df[c].to_numpy(dtype=float))
        flame = band_rms(y, FS, *FLAME_BAND) * lsb * 1000
        alias = band_rms(y, FS, *ALIAS_BAND) * lsb * 1000
        flag = "SIM" if alias > 0.3 * flame and alias > 1.0 else "-"
        ir_res[c] = (g, y, flame)
        print(f"{c:<6}{y.mean():>10.0f}{flame:>13.2f}{alias:>13.2f}{flag:>9}")

    print("\n  Interpretacao: rode o mesmo ensaio com chama e com lampada.")
    print("  Criterio de aprovacao: RMS 5-15 Hz da chama >= 3x o da lampada.")
    print("  Se 'alias?' = SIM, ha 100/120 Hz rebatendo para dentro da banda:")
    print("  monte o RC de 10k+470n em cada AO e repita antes de concluir.\n")

    print("=== SOM (casamento de ganho) ===")
    envs = {}
    for c in SND_COLS:
        g, y = resample(t, df[c].to_numpy(dtype=float))
        e = envelope(y, FS)
        envs[c] = (g, e)
        print(f"{c:<6} envelope: mediana={np.median(e):7.1f}  p95={np.percentile(e,95):7.1f}")

    e1, e2 = envs["som1"][1], envs["som2"][1]
    ativo = e1 + e2 > np.percentile(e1 + e2, 70)   # so onde ha sinal
    ratio = e1[ativo] / np.maximum(e2[ativo], 1e-6)
    med, disp = np.median(ratio), np.percentile(ratio, 90) / np.percentile(ratio, 10)

    print(f"\n  razao som1/som2: mediana={med:.2f}  dispersao(p90/p10)={disp:.2f}")
    if 0.8 < med < 1.25 and disp < 1.5:
        print("  -> ganhos casam. Comparacao direcional e viavel.")
    else:
        print("  -> ganhos NAO casam. Reajuste os trimpots com a mesma fonte")
        print("     e repita. Se nao convergir, abandone o Y de som (o de IR")
        print("     e independente e continua valendo).")

    fig, ax = plt.subplots(len(IR_COLS) + 1, 2, figsize=(13, 3 * (len(IR_COLS) + 1)))
    for i, c in enumerate(IR_COLS):
        g, y, _ = ir_res[c]
        ax[i][0].plot(g, y, lw=0.6); ax[i][0].set_ylabel(f"{c} (cont)")
        f, p = welch(y - y.mean(), FS, nperseg=min(1024, len(y)))
        ax[i][1].semilogy(f, p, lw=0.8)
        ax[i][1].axvspan(*FLAME_BAND, color="orange", alpha=0.25)
        ax[i][1].axvspan(*ALIAS_BAND, color="red", alpha=0.15)
        ax[i][1].set_ylabel("PSD")
    ax[-1][0].plot(envs["som1"][0], e1, lw=0.7, label="som1")
    ax[-1][0].plot(envs["som2"][0], e2, lw=0.7, label="som2")
    ax[-1][0].legend(); ax[-1][0].set_xlabel("s"); ax[-1][0].set_ylabel("envelope")
    ax[-1][1].plot(ratio, lw=0.5); ax[-1][1].axhline(1.0, color="k", ls="--")
    ax[-1][1].set_ylabel("som1/som2"); ax[-1][1].set_ylim(0, 3)
    fig.tight_layout()
    fig.savefig(f"{args.out}.png", dpi=110)
    print(f"\ngrafico: {args.out}.png  (faixa laranja=chama, vermelha=aliasing)")


if __name__ == "__main__":
    main()
