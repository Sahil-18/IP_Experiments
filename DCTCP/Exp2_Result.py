# This file is used to plot the result of experiment 2
# The result is the throughput of multiple DCTCP flows after 0.1 seconds

import matplotlib.pyplot as plt
import numpy as np

data = np.genfromtxt("throughput.dat", skip_header=1, delimiter='\t', dtype=float)

flow = {}
for i in range(len(data)):
    if data[i][1] not in flow:
        flow[data[i][1]] = [[data[i][0], data[i][2]]]
    else:
        flow[data[i][1]].append([data[i][0], data[i][2]])

# Plot the result
plt.figure()
for key in flow:
    x = [i[0] for i in flow[key]]
    y = [i[1] for i in flow[key]]
    plt.plot(x, y, label='Flow ' + str(int(key)))
plt.xlabel('Time (s)')
plt.ylabel('Throughput (Mbps)')
plt.legend()
plt.title('Throughput of Multiple DCTCP Flows')
plt.savefig('Exp2_Throughput.png')

# Further is the queue size of one switch in the network
data = np.genfromtxt("queue_sizes.dat", skip_header=1, delimiter='\t', dtype=float)
queue = []
for i in range(len(data)):
    queue.append([data[i][0], data[i][1]])

# Plot the result
plt.figure()
x = [i[0] for i in queue]
y = [i[1] for i in queue]
plt.plot(x, y)
plt.xlabel('Time (s)')
plt.ylabel('Queue Size (packets)')
plt.title('Queue Size of One Switch in the Network')
plt.savefig('Exp2_Queue.png')
    
