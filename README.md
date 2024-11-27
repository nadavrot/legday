# legday

This is a toy lossless compressor for ML tensors. It's not intended for
production use. The compressor operates in two phases. First, a preprocessing
step that transforms the tensor into a format that is more amenable to
compression. Second, a compression step that uses an arithmetic coder
to compress the tensor.

The repository contains a library and a command-line
utility.

The command-line utility can be used to compress and decompress
tensors. It can also be used to verify that the compression is
lossless. Usage: 

```
legday [compress|decompress|verify] [INT8|BF16|FP32] <input> <output>
```

The repo contains a script to dump the weights from a PyTorch model. You'll 
need to install the pytorch environment using the script in `scripts/create_venv.sh`. Usage:

```
python scripts/dump_weights.py <path to model>
```

