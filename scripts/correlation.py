import numpy as np
import matplotlib.pyplot as plt

SIZE=16

# Fill this with SIZE x SIZE elements that represent the correlation between values.
data = np.array([

])


matrix = data.reshape(SIZE, SIZE)

# Compute correlation matrix
correlation = np.corrcoef(matrix)

# Plot the heatmap
plt.figure(figsize=(SIZE, SIZE))
plt.imshow(correlation, cmap="coolwarm", interpolation="nearest", vmin=-1, vmax=1)
plt.colorbar(label="Correlation")
plt.title("Correlation")

for i in range(SIZE):
  for j in range(SIZE):
    plt.text(i, j, f'{data[j*SIZE+i]:.3f}', ha='center', va='center', color='black')

ticks = np.arange(SIZE)
labels = ticks
plt.xticks(ticks, labels)
plt.yticks(ticks, labels)
plt.show()
