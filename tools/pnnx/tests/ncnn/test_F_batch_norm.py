# Copyright 2021 Tencent
# SPDX-License-Identifier: BSD-3-Clause

import torch
import torch.nn as nn
import torch.nn.functional as F

class Model(nn.Module):
    def __init__(self):
        super(Model, self).__init__()

        self.m3 = torch.rand(16)
        self.v3 = torch.rand(16)
        self.w3 = nn.Parameter(torch.rand(16))
        self.b3 = nn.Parameter(torch.rand(16))
        self.m4 = torch.rand(2)
        self.v4 = torch.rand(2)
        self.w4 = nn.Parameter(torch.rand(2))
        self.b4 = nn.Parameter(torch.rand(2))
        self.m5 = torch.rand(3)
        self.v5 = torch.rand(3)
        self.w5 = nn.Parameter(torch.rand(3))
        self.b5 = nn.Parameter(torch.rand(3))

    def forward(self, x, y, z):
        x = F.batch_norm(x, self.m3, self.v3, self.w3, self.b3)

        y = F.batch_norm(y, self.m4, self.v4, self.w4, self.b4, eps=1e-3)

        z = F.batch_norm(z, self.m5, self.v5, self.w5, self.b5, eps=1e-2)
        return x, y, z

def test():
    net = Model()
    net.eval()

    torch.manual_seed(0)
    x = torch.rand(1, 16)
    y = torch.rand(1, 2, 16)
    z = torch.rand(1, 3, 12, 16)

    a = net(x, y, z)

    # export torchscript
    mod = torch.jit.trace(net, (x, y, z))
    mod.save("test_F_batch_norm.pt")

    # torchscript to pnnx
    import os
    os.system("../../src/pnnx test_F_batch_norm.pt inputshape=[1,16],[1,2,16],[1,3,12,16]")

    # ncnn inference
    import test_F_batch_norm_ncnn
    b = test_F_batch_norm_ncnn.test_inference()

    for a0, b0 in zip(a, b):
        if not torch.allclose(a0, b0, 1e-4, 1e-4):
            return False
    return True

if __name__ == "__main__":
    if test():
        exit(0)
    else:
        exit(1)
