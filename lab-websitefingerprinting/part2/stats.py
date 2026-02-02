import json

def load_traces():
    traces = open("./traces-2-websites.out", "r")

    # Parse the first line as json to get the metadata
    metadata = json.loads(traces.readline())
    # Calculate mean, median, standard deviation of each array
    for i in range(len(metadata["traces"])):
        trace = metadata["traces"][i]
        label = metadata["labels"][i]
        size = len(trace)
        mean = sum(trace) / size
        sorted_trace = sorted(trace)
        median = (sorted_trace[size // 2] if size % 2 == 1 else
                  (sorted_trace[size // 2 - 1] + sorted_trace[size // 2]) / 2)
        variance = sum((x - mean) ** 2 for x in trace) / size
        stddev = variance ** 0.5
        print(f"Trace {i} (Label: {label}): Mean={mean}, Median={median}, StdDev={stddev}")

if __name__ == "__main__":
    load_traces()
