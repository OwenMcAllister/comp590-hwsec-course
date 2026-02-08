import json
from urllib.parse import urlparse

import numpy as np

TARGET_DOMAINS = {"google.com", "nytimes.com"}


def domain_from_label(label: str) -> str:
    # Labels are URLs like "https://www.google.com"
    host = urlparse(label).hostname or ""
    if host.startswith("www."):
        host = host[4:]
    return host


with open("traces4stats.out", "r") as f:
    data = json.loads(f.read())

    traces = np.array(data["traces"])
    labels = data.get("labels", [])

if not labels:
    raise SystemExit("No labels found. Re-run automation to include labels.")

print("Trace Number ---- Domain ----- Samples ----- Mean ----- Median ------ StdDev ------ Min ------ Max")

trace_counter = 0
for domain in sorted(TARGET_DOMAINS):
    idx = [i for i, label in enumerate(labels) if domain_from_label(label) == domain]
    if not idx:
        continue
    # Each trace is an array of timing measurements. We compute statistics for each trace and print them.
    domain_traces = traces[idx]
    for trace in domain_traces:
        trace_counter += 1
        mean = trace.mean()
        median = np.median(trace)
        std = trace.std(ddof=1)
        min_v = trace.min()
        max_v = trace.max()
        print(
            f"{trace_counter:<12} ---- {domain:<9} ----- {trace.size:<7} ----- "
            f"{mean:>8.4f} ----- {median:>8.4f} ------ {std:>8.4f} ------ {min_v:>8.4f} ------ {max_v:>8.4f}"
        )
