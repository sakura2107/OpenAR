"""Fetch license-verified official Paddle weights at fixed revisions and export ONNX.

Run only in an isolated conversion environment. Does not interact with a device.
The exported files are candidates until corpus evaluation; old models are retained.
"""
from __future__ import annotations
import argparse
import concurrent.futures
import hashlib
import importlib.metadata
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import urllib.request

MODELS = {
    "chinese_cht_PP-OCRv3_mobile_rec": "ab3009d849dea4d7296aabce47acff1ab124ebff",
    "PP-OCRv5_mobile_rec": "682f20538d8c086cb2128e5cfac775e6c4904e85",
    "PP-OCRv5_mobile_det": "0d63e78e2b680928f6b1747d76a08db6e645efb7",
    "japan_PP-OCRv3_mobile_rec": "199d71fd22b19f6964075e6164a4cf8ca23d0178",
    "en_PP-OCRv4_mobile_rec": None,
    "PP-OCRv5_server_rec": None,
}
PADDLE_REF = "8cce9b6fd7ccb50226d0c38f94054d81c29b8184"


def fetch(url, path):
    if path.exists():
        return
    request = urllib.request.Request(url, headers={"User-Agent": "OpenAR-B1-model-audit"})
    for attempt in range(4):
        try:
            with urllib.request.urlopen(request, timeout=45) as response:
                data = response.read()
            break
        except (OSError, urllib.error.URLError):
            if attempt == 3: raise
            time.sleep(1 + attempt)
    temporary = path.with_name(path.name + ".partial")
    temporary.write_bytes(data)
    temporary.replace(path)


def download(name, revision, sources):
    api = f"https://huggingface.co/api/models/PaddlePaddle/{name}"
    metadata = json.load(urllib.request.urlopen(api + (f"/revision/{revision}" if revision else ""), timeout=30))
    if metadata.get("cardData", {}).get("license") != "apache-2.0":
        raise ValueError(f"{name}: specific weight license is not verified Apache-2.0")
    directory = sources / name
    directory.mkdir(parents=True, exist_ok=True)
    lock = directory / "source.json"
    if lock.exists():
        revision = json.loads(lock.read_text(encoding="utf-8"))["revision"]
    else:
        revision = metadata["sha"]
    for filename in ("inference.json", "inference.pdiparams", "inference.yml", "README.md", "config.json"):
        fetch(f"https://huggingface.co/PaddlePaddle/{name}/resolve/{revision}/{filename}", directory / filename)
    record = {"repo": f"PaddlePaddle/{name}", "revision": revision, "license": "Apache-2.0",
              "source": f"https://huggingface.co/PaddlePaddle/{name}/tree/{revision}",
              "files": {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in directory.iterdir() if p.is_file() and p.name != "source.json"}}
    lock.write_text(json.dumps(record, indent=2), encoding="utf-8")
    return name, directory, record


def prepare(root, sources, selected=None):
    import yaml
    import onnx
    target = root / "models/official"
    target.mkdir(parents=True, exist_ok=True)
    sources.mkdir(parents=True, exist_ok=True)
    licenses = root / "docs/licenses/paddleocr"
    licenses.mkdir(parents=True, exist_ok=True)
    fetch(f"https://raw.githubusercontent.com/PaddlePaddle/PaddleOCR/{PADDLE_REF}/LICENSE", licenses / "LICENSE")
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        selected = selected or [name for name in MODELS if name != "PP-OCRv5_server_rec"]
        results = list(pool.map(lambda name: download(name, MODELS[name], sources), selected))
    for name, directory, provenance in results:
        output = target / (name + ".onnx")
        command = [sys.executable, "-m", "paddle2onnx.command", "--model_dir", str(directory),
                   "--model_filename", "inference.json", "--params_filename", "inference.pdiparams",
                   "--save_file", str(output), "--opset_version", "17", "--enable_onnx_checker", "True",
                   "--optimize_tool", "None"]
        if not output.exists():
            environment = dict(os.environ, FLAGS_enable_pir_api="1")
            result = subprocess.run(command, env=environment, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            (directory / "conversion.log").write_bytes(result.stdout)
            if result.returncode:
                print(result.stdout.decode("utf-8", errors="replace"))
                raise RuntimeError(f"Conversion failed for {name}: {result.returncode}")
        model = onnx.load(str(output))
        onnx.checker.check_model(model)
        config = yaml.safe_load((directory / "inference.yml").read_text(encoding="utf-8"))
        characters = config.get("PostProcess", {}).get("character_dict")
        if characters is not None:
            if not isinstance(characters, list) or any(not isinstance(c, str) or not c or "\n" in c or "\r" in c for c in characters):
                raise ValueError(f"{name}: unsupported dictionary")
            (target / (name + ".txt")).write_text("\n".join(characters) + "\n", encoding="utf-8", newline="\n")
        provenance["conversion"] = {
            "paddlepaddle": importlib.metadata.version("paddlepaddle"),
            "paddle2onnx": importlib.metadata.version("paddle2onnx"),
            "onnx": importlib.metadata.version("onnx"), "opset_requested": 17,
            "command": ["python"] + command[1:], "modified": "Paddle PIR graph converted to ONNX; weights retained",
            "onnx_sha256": hashlib.sha256(output.read_bytes()).hexdigest(),
            "actual_opsets": [{"domain": p.domain, "version": p.version} for p in model.opset_import]}
        if characters is not None:
            provenance["dictionary"] = {"source": "official inference.yml PostProcess.character_dict, order preserved",
                                        "rows": len(characters), "sha256": hashlib.sha256((target / (name + ".txt")).read_bytes()).hexdigest()}
        (target / (name + ".provenance.json")).write_text(json.dumps(provenance, indent=2), encoding="utf-8")
        (licenses / (name + ".model-card.md")).write_bytes((directory / "README.md").read_bytes())
        print(f"Prepared candidate {output.name}, dict={len(characters) if characters is not None else 'detector'}", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--sources", type=Path, required=True)
    parser.add_argument("--models", nargs="+", choices=tuple(MODELS), help="Prepare only these models; default retains the B1 mobile set")
    arguments = parser.parse_args()
    prepare(arguments.root.resolve(), arguments.sources.resolve(), arguments.models)
