#!/usr/bin/env python
import sys, os
import torch

device = torch.device('cpu')

if len(sys.argv) != 2:
    print("Usage: python load_model.py <path to model>")
    sys.exit(1)

if os.path.exists(sys.argv[1]) == False:
    print("File does not exist")
    sys.exit(1)

model = torch.load(sys.argv[1], weights_only=True, map_location=device)

def make_safe_name(name):
    for i in range(len(name)):
        if name[i] not in "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.":
            name[i] = "_"

#iterate over the tensors in the model and save them to a file.
for tensor_name in model:
    tensor = model[tensor_name]
    bytes = tensor.cpu().contiguous().view(torch.uint8).numpy().tobytes()
    # remove invalid characters from the tensor name
    make_safe_name(tensor_name)
    #save the contents of the tensor to a file
    with open(sys.argv[1] + "." + tensor_name + "." + str(tensor.dtype) + ".bin", "wb") as f:
        f.write(bytes)
