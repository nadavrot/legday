import sys
import torch


device = torch.device('cpu')

if len(sys.argv) != 1:
    print("Usage: python load_model.py <path to model>")
    sys.exit(1)

model = torch.load("data/llama-32-1b.pth", weights_only=True, device=device)

for element in model:
    print(element)
