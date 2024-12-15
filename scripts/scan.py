#!/bin/env python
import sys

if len(sys.argv) != 2:
    print("Usage: python scan.py <path to model>")
    sys.exit(1)

def load_as_int32_array(filepath):
    with open(filepath, "rb") as f:
        content = f.read()
    int_array = [int.from_bytes(content[i:i+4], byteorder='little', signed=True) 
                 for i in range(0, len(content), 4)]
    return int_array

int_array = load_as_int32_array(sys.argv[1])

def transform_array_with_xor(arr):
    prev = 0
    for i in range(len(arr)):
        temp = arr[i]
        arr[i] ^= prev
        prev = temp

def create_and_visualize_histogram(int_array):
    histogram = {}
    for value in int_array:
        val = (value >> (22-8)) & 0xff
        histogram[val] = histogram.get(val, 0) + 1

    #visualize the histogram
    import matplotlib.pyplot as plt
    plt.bar(histogram.keys(), histogram.values())
    plt.show()


# calculate the correlation between bits
def get_nth_bit(value, n):
    return (value >> n) & 1


def print_correlation_prev_word(arr):
    # return an array with 32 float values between zero and one that represent
    # the correlation between each bit and the previous bit
    ones = [0] * 32
    for i in range(10, len(arr)):
        for j in range(32):
            ones[j] += get_nth_bit(arr[i], j) ^ get_nth_bit(arr[i-1], j)
            
    # normalize the values
    for i in range(32): ones[i] = ones[i] / len(arr)

    cnt = 0
    for val in ones:
      print("{:.2f}".format(val), end=" ")
      if cnt % 8 == 7: print()
      cnt += 1


def print_correlation_in_word(arr, num_bits=25, arr_len=100000):
    # print the correlation matrix between bits in the same word
    ones = [[0] * num_bits for _ in range(num_bits)]
    for i in range(0, arr_len):
        val = arr[i]
        for j in range(num_bits):
            bit = get_nth_bit(val, j)
            for k in range(num_bits):
                ones[j][k] += bit ^ get_nth_bit(val, k)

    # normalize the values
    for i in range(num_bits):
        for j in range(num_bits):
            ones[i][j] = ones[i][j] / arr_len

    print("rendering")
    print(ones)

    #plot the matrix
    import matplotlib.pyplot as plt
    plt.imshow(ones, cmap="coolwarm", interpolation="nearest", vmin=0, vmax=1)
    plt.colorbar(label="Correlation")
    plt.title("Correlation")
    plt.show()

def print_correlation_in_word(arr, skip=0, train_len=100000):
  bins = 4
  print("taking bit ", skip + bins)
  # use the first n bits to train a model that predicts the probability of the
  # next bit using a histogram.
  ones = [0 for _ in range(2**bins)]
  total = [1 for _ in range(2**bins)]
  for i in range(0, train_len):
    val = arr[i]
    # take 'bins' bits before our target bit    
    val = val >> skip
    key = val & 0xf
    val = val >> 4
    target = val & 1
    ones[key] += target
    total[key] += 1

  histogram = [ones[i] / total[i] for i in range(2**bins)]
  print(histogram)
  import matplotlib.pyplot as plt
  plt.bar(range(2**bins), histogram)
  plt.show()


for i in range(0, 24):
  print_correlation_in_word(int_array, skip = i, train_len=len(int_array))
