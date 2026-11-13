#!/usr/bin/env python3
# P23: Exporta htdemucs (demucs 4.1.0, PyTorch) a un unico ONNX fp32 valido para
# onnxruntime CPU. Requiere: pip3 install demucs torch onnx onnxruntime
#
# Salida: assets/models/htdemucs_fp32.onnx (~171 MB, archivo unico, opset 20,
# entrada fija [1, 2, 343980] = segmento de entrenamiento 39/5 s a 44.1 kHz,
# salida [1, 4, 2, 343980] = 4 stems x 2 canales).
#
# El grafico resultante es 100% real (sin complex64): el STFT forward se
# reimplementa con framing + torch.fft.rfft (los kernels int8/fp16 de ORT CPU
# resultaron mas lentos y peores en calidad, ver verificacion abajo), y la
# salida _mask (cac=True) se emite como pares re/im en vez de view_as_complex.
# Ademas se corrigen los indices int32 de ScatterND (ORT los exige int64).
#
# Verificacion: compara la salida ORT contra el forward eager de demucs
# (verdad de terreno) y exige cos > 0.999 y err relativo < 1e-2.
#
# Uso: python3 tools/export_stems_model.py [--out assets/models/htdemucs_fp32.onnx]

import argparse
import math
import os
import sys

import numpy as np
import torch
import torch.nn.functional as F

import demucs.spec as dspec
import demucs.htdemucs as hdmod
from demucs.pretrained import get_model

# ---------------------------------------------------------------- patches ---

# th.stft (normalizado) dividio complex/float: ONNX no soporta Division con
# complex64. STFT manual: frames por unfold + hann + rfft. El factor 1/64
# reproduce exactamente la normalizacion normalized=True de torch.
STFT_NORM = 0.015625


def spectro_patched(x, n_fft=512, hop_length=None, pad=0):
    *other, length = x.shape
    x = x.reshape(-1, length)
    hop = hop_length or n_fft // 4
    win = torch.hann_window(n_fft).to(x)
    xp = F.pad(x, (n_fft // 2, n_fft // 2), mode='reflect')
    frames = xp.unfold(-1, n_fft, hop)
    z = torch.fft.rfft(frames * win, dim=-1).transpose(1, 2) * STFT_NORM
    _, freqs, frame = z.shape
    return z.view(*other, freqs, frame)


# demucs.pad1d tiene asserts data-dependent que torch.export no soporta.
def pad1d_noassert(x, paddings, mode='constant', value=0.):
    length = x.shape[-1]
    padding_left, padding_right = paddings
    if mode == 'reflect':
        max_pad = max(padding_left, padding_right)
        if length <= max_pad:
            extra_pad = max_pad - length + 1
            extra_pad_right = min(padding_right, extra_pad)
            extra_pad_left = extra_pad - extra_pad_right
            paddings = (padding_left - extra_pad_left, padding_right - extra_pad_right)
            x = F.pad(x, (extra_pad_left, extra_pad_right))
    return F.pad(x, paddings, mode, value)


# htdemucs usa cac=True: _mask nunca usa z ni wiener; solo reinterpreta los
# 16 canales del decoder como pares re/im. Emitimos los pares sin
# view_as_complex para que el grafo quede 100% real.
def _mask_realpairs(self, z, m):
    if not self.cac:
        raise RuntimeError("export requiere el modelo htdemucs estandar (cac=True)")
    B, S, C, Fr, T = m.shape
    return m.view(B, S, -1, 2, Fr, T).permute(0, 1, 2, 4, 5, 3)  # (B,S,C,F,T,2)


# istft manual: torch.istft siempre termina en irfft (DFT onesided+inverse,
# que ORT < 1.23 rechaza). ifft(hermitian_ext, norm="ortho").real es
# matematicamente identico al irfft; el resto replica torch/_refs istft
# (OLA por unfold_backward + envolvente win^2). Verificado vs torch.istft
# con err max 1e-6.
def istft_manual(zc, n_fft, hop, window, length, center=True):
    N, freqs, T = zc.shape
    full = torch.cat([zc, torch.conj(torch.flip(zc[:, 1:-1, :], dims=[1]))], dim=1)
    frames = torch.fft.ifft(full, dim=1, norm="ortho").real
    frames = frames.transpose(1, 2)
    y_tmp = frames * window.view(1, 1, -1)
    out_len = n_fft + hop * (T - 1)
    y = torch.ops.aten.unfold_backward(y_tmp, input_sizes=(N, out_len), dim=1, size=n_fft, step=hop)
    env = torch.ops.aten.unfold_backward(window.pow(2).expand(N, T, n_fft),
                                         input_sizes=(N, out_len), dim=1, size=n_fft, step=hop)
    start = n_fft // 2 if center else 0
    y = y[:, start:start + length]
    env = env[:, start:start + length]
    return y / env


# _ispec original hace F.pad sobre tensores complex (aten.pad no se descompone
# para complex). Con pares re/im el pad va en dims -3 (F) y -2 (T).
def _ispec_realpairs(self, z, length=None, scale=0):
    hl = self.hop_length // (4 ** scale)

    def pad_cat(t, dim, left, right):
        shape = list(t.shape)
        shape[dim] = left
        zl = torch.zeros(shape, dtype=t.dtype, device=t.device)
        shape[dim] = right
        zr = torch.zeros(shape, dtype=t.dtype, device=t.device)
        return torch.cat([zl, t, zr], dim=dim)

    z = pad_cat(z, -3, 0, 1)   # F +1 abajo (igual que F.pad(z, (0, 0, 0, 1)))
    z = pad_cat(z, -2, 2, 2)   # T +-2    (igual que F.pad(z, (2, 2)))
    pad = hl // 2 * 3
    le = hl * int(math.ceil(length / hl)) + 2 * pad
    *other, freqs, frames, two = z.shape
    n_fft = 2 * freqs - 2
    zc = torch.complex(z[..., 0], z[..., 1]).reshape(-1, freqs, frames)
    xr = istft_manual(zc, n_fft, hl,
                      torch.hann_window(n_fft).to(z.real), le, center=True)
    x = xr.view(*other, le)
    return x[..., pad:pad + length]


# ------------------------------------------------------------ fixup ONNX ---

def fix_scatternd_indices(path):
    """ScatterND/GatherElements con indices int32 -> Cast a int64 (ORT exige)."""
    import onnx
    from onnx import TensorProto, helper

    m = onnx.load(path)
    g = m.graph
    dt_in = {}
    for init in g.initializer:
        dt_in[init.name] = init.data_type
    for vi in list(g.input):
        if vi.type.tensor_type.elem_type:
            dt_in[vi.name] = vi.type.tensor_type.elem_type

    dt = dict(dt_in)
    for n in g.node:
        known = [dt.get(i) for i in n.input if i and dt.get(i) is not None]
        out_t = None
        if n.op_type == "Cast":
            for a in n.attribute:
                if a.name == "to":
                    out_t = a.i
        elif n.op_type == "ConstantOfShape":
            out_t = TensorProto.FLOAT
            for a in n.attribute:
                if a.HasField("value") and a.value:
                    out_t = a.value.data_type
        elif n.op_type in ("Shape", "NonZero", "Size"):
            out_t = TensorProto.INT64
        elif known:
            out_t = known[0]
        for o in n.output:
            if o:
                dt[o] = out_t

    total = 0
    new_nodes = []
    for n in g.node:
        inputs = list(n.input)
        if n.op_type in ("ScatterND", "ScatterElements", "GatherElements", "GatherND"):
            if inputs[1] and dt.get(inputs[1]) == TensorProto.INT32:
                co = f"{inputs[1]}_i64_{total}"
                new_nodes.append(helper.make_node("Cast", [inputs[1]], [co],
                                                  to=TensorProto.INT64, name=co + "_cast"))
                inputs[1] = co
                total += 1
        n.input[:] = inputs
        new_nodes.append(n)
    del g.node[:]
    g.node.extend(new_nodes)
    onnx.save(m, path)
    # onnx.save inlina los pesos; borra el .data externo si el exportador lo dejó.
    if not any(t.data_location == TensorProto.EXTERNAL for t in g.initializer):
        ext = path + ".data"
        if os.path.exists(ext):
            os.remove(ext)
    return total


# ------------------------------------------------------------------ main ---

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="assets/models/htdemucs_fp32.onnx")
    ap.add_argument("--skip-verify", action="store_true")
    args = ap.parse_args()

    torch.manual_seed(0)
    bag = get_model('htdemucs')
    model = bag.models[0]
    model.eval()
    model.use_train_segment = True
    seg = int(model.segment * model.samplerate)
    print(f"modelo: htdemucs  segmento={seg} muestras  cac={model.cac}")

    x = torch.randn(1, 2, seg) * 0.3
    with torch.no_grad():
        ref_original = model(x.clone()).numpy()

    dspec.spectro = spectro_patched
    hdmod.spectro = spectro_patched
    hdmod.pad1d = pad1d_noassert
    hdmod.HTDemucs._mask = _mask_realpairs
    hdmod.HTDemucs._ispec = _ispec_realpairs

    with torch.no_grad():
        ref = model(x.clone()).numpy()
    eq = np.abs(ref - ref_original).max() / (np.abs(ref_original).max() + 1e-9)
    print(f"forward ok; parches vs demucs original: err rel={eq:.2e}")
    if eq > 1e-4:
        print("FALLO: los parches no reproducen el modelo original", file=sys.stderr)
        sys.exit(1)

    with torch.no_grad():
        torch.onnx.export(model, (x,), args.out, dynamo=True)
    n_fix = fix_scatternd_indices(args.out)
    print(f"export ok ({n_fix} fixups ScatterND)  tamaño: "
          f"{os.path.getsize(args.out) / 1e6:.0f} MB")

    if args.skip_verify:
        return

    import onnxruntime as ort
    sess = ort.InferenceSession(args.out, providers=["CPUExecutionProvider"])
    out = sess.run(None, {sess.get_inputs()[0].name: x.numpy()})[0]
    err = np.abs(out - ref)
    rel = err.max() / (np.abs(ref).max() + 1e-9)
    cos = np.dot(out.ravel(), ref.ravel()) / (
        np.linalg.norm(out.ravel()) * np.linalg.norm(ref.ravel()) + 1e-12)
    t0 = time.time()
    sess.run(None, {sess.get_inputs()[0].name: x.numpy()})
    dt = time.time() - t0
    print(f"verificacion ORT: rel={rel:.2e}  cos={cos:.6f}  "
          f"{seg / model.samplerate:.1f}s audio en {dt:.1f}s ({seg / model.samplerate / dt:.2f}x rt)")
    if cos < 0.999 or rel > 1e-2:
        print("FALLO: la verificacion numerica no alcanza el umbral", file=sys.stderr)
        sys.exit(1)
    print("OK")


if __name__ == "__main__":
    import time
    main()
