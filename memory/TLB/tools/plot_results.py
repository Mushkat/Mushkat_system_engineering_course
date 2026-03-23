import csv
import sys
from pathlib import Path

import matplotlib.pyplot as plt


def main():
    if len(sys.argv) not in (2, 3):
        print("Usage: python plot_results.py <input_csv> [output_png]")
        return 1

    input_csv = Path(sys.argv[1])
    output_png = Path(sys.argv[2]) if len(sys.argv) == 3 else Path("tlb_plot.png")

    pages = []
    ns = []

    with input_csv.open("r", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            pages.append(int(row["pages"]))
            ns.append(float(row["ns_per_access"]))

    plt.figure(figsize=(9, 5))
    plt.plot(pages, ns, marker="o")
    plt.xscale("log", base=2)
    plt.xlabel("Number of pages")
    plt.ylabel("ns per access")
    plt.title("TLB benchmark")
    plt.grid(True)
    plt.tight_layout()
    plt.savefig(output_png, dpi=150)
    print(f"Saved plot to {output_png}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())