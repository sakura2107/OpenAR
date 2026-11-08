"""Read-only ONNX/dictionary inventory. Does not evaluate OCR accuracy.

Python dependencies: onnx==1.19.1; optional --probe: onnxruntime==1.23.2, numpy.
Production OpenAR uses its own native ORT; probe timings are deliberately omitted.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path


def dictionary(path: Path) -> dict:
    data = path.read_bytes()
    try:
        text = data.decode("utf-8-sig", errors="strict")
        encoding = "utf-8"
    except UnicodeDecodeError:
        text = data.decode("gb18030", errors="strict")
        encoding = "gb18030"
    # Match std::getline: only LF splits rows, a terminal LF adds no row.
    lines = text.split("\n")
    if lines and lines[-1] == "":
        lines.pop()
    lines = [line.removesuffix("\r") for line in lines]
    if not lines or any(not line or "\0" in line for line in lines):
        raise ValueError(f"{path}: empty/NUL dictionary entry")
    return {"encoding": encoding, "rows": len(lines), "literal_space_indices": [
        i + 1 for i, line in enumerate(lines) if line == " "],
        "duplicates": len(lines) - len(set(lines)),
        "sha256": hashlib.sha256(data).hexdigest()}


def inventory(root: Path, probe: bool = False, references: Path | None = None) -> dict:
    import onnx
    result = {"schema_version": 1, "purpose": "model-contract audit; not BA acceptance",
              "models": [], "dictionaries": {}}
    for path in sorted((root / "models").glob("*.txt")):
        result["dictionaries"][path.name] = dictionary(path)
        if references and (references / path.name).is_file():
            original = (references / path.name).read_bytes()
            local = path.read_bytes()
            encoding = result["dictionaries"][path.name]["encoding"]
            actual = local.decode("utf-8-sig" if encoding == "utf-8" else encoding).splitlines()
            expected = original.decode("utf-8-sig").splitlines()
            result["dictionaries"][path.name]["reference_comparison"] = {
                "reference_sha256": hashlib.sha256(original).hexdigest(),
                "reference_rows": len(expected),
                "different_indices": [{"class_index": i + 1, "local": a, "reference": b}
                                      for i, (a, b) in enumerate(zip(actual, expected)) if a != b],
                "same_row_count": len(actual) == len(expected)}
    for path in sorted((root / "models").glob("*.onnx")):
        model = onnx.load(str(path), load_external_data=False)
        def tensor(value):
            return {"name": value.name,
                    "dtype": onnx.TensorProto.DataType.Name(value.type.tensor_type.elem_type),
                    "shape": [d.dim_value if d.HasField("dim_value") else d.dim_param
                              for d in value.type.tensor_type.shape.dim]}
        initializers = {value.name for value in model.graph.initializer}
        entry = {"file": path.name, "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                 "producer": model.producer_name, "producer_version": model.producer_version,
                 "opsets": [{"domain": v.domain, "version": v.version} for v in model.opset_import],
                 "inputs": [tensor(v) for v in model.graph.input if v.name not in initializers],
                 "outputs": [tensor(v) for v in model.graph.output]}
        if probe:
            import numpy as np
            import onnxruntime as ort
            options = ort.SessionOptions()
            options.intra_op_num_threads = 2
            options.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_EXTENDED
            session = ort.InferenceSession(str(path), options, providers=["CPUExecutionProvider"])
            recognition = path.name.startswith("rec")
            shape = (1, 3, 48, 320) if recognition else (1, 3, 96, 128)
            outputs = session.run(None, {session.get_inputs()[0].name: np.zeros(shape, dtype=np.float32)})
            entry["probe"] = {"runtime": ort.__version__, "provider": "CPUExecutionProvider",
                              "input": shape, "outputs": [list(v.shape) for v in outputs],
                              "outputs_finite": all(bool(np.isfinite(v).all()) for v in outputs)}
        result["models"].append(entry)
    checks = []
    for model_name, dictionary_name in [("rec.sim.onnx", "ppocr_keys_v1.txt"),
                                         ("rec_ja.onnx", "japan_dict.txt")]:
        entry = next(v for v in result["models"] if v["file"] == model_name)
        count = result["dictionaries"][dictionary_name]["rows"]
        classes = entry["outputs"][0]["shape"][-1]
        checks.append({"model": model_name, "dictionary": dictionary_name,
                       "dictionary_rows": count, "classes": classes,
                       "matches_blank_plus_appended_space": classes == count + 2,
                       "fixed_input_height": entry["inputs"][0]["shape"][2]})
    result["existing_pairs"] = checks
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--probe", action="store_true")
    parser.add_argument("--reference-dicts", type=Path)
    args = parser.parse_args()
    report = inventory(args.root, args.probe, args.reference_dicts)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    # Reports are new artifacts; do not accidentally replace manual annotations.
    with args.output.open("x", encoding="utf-8", newline="\n") as file:
        json.dump(report, file, indent=2, ensure_ascii=False)
        file.write("\n")
    print(f"Model audit written: {args.output}")
