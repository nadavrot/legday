#!/bin/bash

for f in *bfloat16*; do echo $f; ../build/utility/legday verify BF16 $f /tmp/out.bin; done
for f in *float32*; do echo $f; ../build/utility/legday verify FP32 $f /tmp/out.bin; done
