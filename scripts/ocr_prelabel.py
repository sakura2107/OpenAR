"""Run OpenAR's existing native OCR plugin on captured PNGs, offline.

Only produces UNREVIEWED predictions. This calls the production C++ plugin
and native ORT, not a Python reimplementation. No device APIs are loaded.
Use --profile for configured preprocessing, or explicit models for a legacy baseline.
This command prepares proposals, never ground truth or an accuracy test.
Dependencies: numpy, opencv-python-headless. Windows x64 Python required.
"""
from __future__ import annotations
import argparse
import ctypes as C
import hashlib
import json
import os
from pathlib import Path
import time
from ocr_corpus import read_jsonl, safe_image, image_info


class Params(C.Structure):
    _fields_ = [("det_model", C.c_char_p), ("rec_model", C.c_char_p), ("dictionary", C.c_char_p),
                ("text_threshold", C.c_float), ("box_threshold", C.c_float), ("unclip_ratio", C.c_float),
                ("score_mode", C.c_int32), ("device_id", C.c_int32)]


class ParamsV2(C.Structure):
    _fields_ = [("struct_size", C.c_uint32), ("base", Params), ("det_line_merge_ratio", C.c_float)]


class View(C.Structure):
    _fields_ = [("struct_size", C.c_uint32), ("data", C.c_void_p), ("byte_length", C.c_uint64),
                ("stride", C.c_int64), ("width", C.c_int32), ("height", C.c_int32), ("channels", C.c_int32)]


class Rect(C.Structure):
    _fields_ = [("x", C.c_int32), ("y", C.c_int32), ("width", C.c_int32), ("height", C.c_int32)]


class Item(C.Structure):
    _fields_ = [("struct_size", C.c_uint32), ("text", C.c_void_p), ("text_length", C.c_uint64),
                ("score", C.c_float), ("box", Rect)]


class NativeOcr:
    def __init__(self, plugin, det=None, rec=None, dictionary=None, profile=None):
        if C.sizeof(C.c_void_p) != 8:
            raise ValueError("Use x64 Python")
        plugin = plugin.resolve(strict=True)
        self.search = [os.add_dll_directory(str(plugin.parent)), os.add_dll_directory(str(plugin.parent.parent))]
        # cuDNN dynamically loads additional engines using legacy Windows DLL
        # search. add_dll_directory alone does not cover that path in a Python
        # host outside the OpenAR binary directory. Scope PATH to this process.
        self.previous_path = os.environ.get("PATH", "")
        os.environ["PATH"] = str(plugin.parent.parent) + os.pathsep + self.previous_path
        self.dll = C.CDLL(str(plugin))
        d = self.dll
        signatures = {
            "arfw_ocr_create_v2": ([C.POINTER(ParamsV2), C.c_void_p, C.c_void_p, C.c_char_p, C.c_int32], C.c_void_p),
            "arfw_ocr_destroy": ([C.c_void_p], None),
            "arfw_ocr_run_v2": ([C.c_void_p, C.POINTER(View), C.c_void_p, C.POINTER(C.c_void_p), C.c_char_p, C.c_int32], C.c_int32),
            "arfw_ocr_result_count_v2": ([C.c_void_p], C.c_int32),
            "arfw_ocr_result_item_v2": ([C.c_void_p, C.c_int32, C.POINTER(Item)], C.c_int32),
            "arfw_ocr_result_destroy_v2": ([C.c_void_p], None)}
        for name, (arguments, returned) in signatures.items():
            function = getattr(d, name)
            function.argtypes, function.restype = arguments, returned
        error = C.create_string_buffer(1024)
        if profile:
            factory = d.arfw_ocr_create_profile_v3
            factory.argtypes = [C.c_char_p, C.c_int32, C.c_void_p, C.c_void_p, C.c_char_p, C.c_int32]
            factory.restype = C.c_void_p
            self.handle = factory(str(profile.resolve(strict=True)).encode("utf-8"), -1, None, None, error, len(error))
        else:
            params = Params(str(det.resolve(strict=True)).encode("utf-8"), str(rec.resolve(strict=True)).encode("utf-8"),
                            str(dictionary.resolve(strict=True)).encode("utf-8"), 0.5, 0.5, 1.5, 0, -1)
            config = ParamsV2(C.sizeof(ParamsV2), params, 0.5)
            self.handle = d.arfw_ocr_create_v2(C.byref(config), None, None, error, len(error))
        if not self.handle:
            raise RuntimeError(error.value.decode("utf-8", errors="replace"))
        if hasattr(d, "arfw_ocr_run_mode_v3"):
            d.arfw_ocr_run_mode_v3.argtypes = [C.c_void_p, C.POINTER(View), C.c_void_p, C.c_int32, C.POINTER(C.c_void_p),
                                               C.POINTER(C.c_double), C.POINTER(C.c_double), C.c_char_p, C.c_int32]
            d.arfw_ocr_run_mode_v3.restype = C.c_int32
        self.last_timings = {}

    def recognize(self, image, mode=0):
        import numpy as np
        image = np.ascontiguousarray(image)
        h, w, channels = image.shape
        view = View(C.sizeof(View), image.ctypes.data, image.nbytes, image.strides[0], w, h, channels)
        error = C.create_string_buffer(1024)
        result = C.c_void_p()
        try:
            started = time.perf_counter_ns()
            if hasattr(self.dll, "arfw_ocr_run_mode_v3"):
                detection_ms, recognition_ms = C.c_double(), C.c_double()
                status = self.dll.arfw_ocr_run_mode_v3(self.handle, C.byref(view), None, mode, C.byref(result),
                    C.byref(detection_ms), C.byref(recognition_ms), error, len(error))
                self.last_timings = {"detection_ms": detection_ms.value, "recognition_ms": recognition_ms.value}
            else:
                if mode: raise ValueError("Legacy plugin has no crop/numeric route")
                status = self.dll.arfw_ocr_run_v2(self.handle, C.byref(view), None, C.byref(result), error, len(error))
            elapsed = (time.perf_counter_ns() - started) / 1e6
            if status or not result:
                raise RuntimeError(error.value.decode("utf-8", errors="replace"))
            count = self.dll.arfw_ocr_result_count_v2(result)
            if count < 0 or count > 10000:
                raise RuntimeError("Invalid native result count")
            items = []
            for index in range(count):
                item = Item()
                item.struct_size = C.sizeof(Item)
                if self.dll.arfw_ocr_result_item_v2(result, index, C.byref(item)) or not item.text or item.text_length > 1024*1024:
                    raise RuntimeError("Invalid native OCR item")
                items.append({"text": C.string_at(item.text, item.text_length).decode("utf-8", errors="strict"),
                              "score": item.score,
                              "box": [item.box.x, item.box.y, item.box.width, item.box.height]})
            return items, elapsed
        finally:
            if result:
                self.dll.arfw_ocr_result_destroy_v2(result)

    def close(self):
        if self.handle:
            self.dll.arfw_ocr_destroy(self.handle)
            self.handle = None
        os.environ["PATH"] = self.previous_path
        for directory in self.search:
            directory.close()


def prelabel(args):
    import cv2
    import numpy as np
    frames = read_jsonl(args.capture)
    if not frames:
        raise ValueError("Capture is empty")
    # Validate all paths and output location before creating a model session.
    images = []
    for frame in frames:
        image = safe_image(args.capture.parent, frame["image"])
        size, digest = image_info(image)
        if size != (1280, 720) or frame["language"] not in ("ja", "zh-Hant"):
            raise ValueError("Invalid captured frame")
        relative = Path(os.path.relpath(image, args.output.parent)).as_posix()
        safe_image(args.output.parent, relative)
        images.append((frame, image, digest, relative))
    if args.output.exists() or args.output.with_suffix(".run.json").exists():
        raise ValueError("Output already exists; use a new name")
    ocr = NativeOcr(args.plugin, args.det_model, args.rec_model, args.dictionary, args.profile)
    seen, times = set(), []
    try:
        with args.output.open("x", encoding="utf-8", newline="\n") as file:
            for frame, image, digest, relative in images:
                if digest in seen:
                    continue
                seen.add(digest)
                pixels = cv2.imdecode(np.frombuffer(image.read_bytes(), dtype=np.uint8), cv2.IMREAD_COLOR)
                if pixels is None:
                    raise ValueError(f"Image decoding failed: {image}")
                items, elapsed = ocr.recognize(pixels)
                times.append({"image": relative, "native_full_pipeline_ms": elapsed, "regions": len(items)})
                for index, item in enumerate(items, 1):
                    record = {"schema_version": 1, "id": f"{frame['language']}-{digest[:12]}-region-{index:03d}",
                              "language": frame["language"], "image": relative, "image_sha256": digest,
                              "scene": "", "split": "", "kind": "", "number_form": "", "styles": [],
                              "box": item["box"], "text": None, "prediction": item["text"],
                              "reviewed": False, "reviewer": "", "reviewed_at": "",
                              "audited_by": "", "audited_at": ""}
                    file.write(json.dumps(record, ensure_ascii=False) + "\n")
                file.flush()
                print(f"{relative}: {len(items)} unreviewed proposals")
        assets = [args.plugin, args.plugin.resolve().parent.parent / "onnxruntime.dll"]
        assets += [args.profile] if args.profile else [args.det_model, args.rec_model, args.dictionary]
        metadata = {"acceptance": False, "pipeline": "OpenAR native configured profile" if args.profile else "OpenAR native legacy baseline", "times": times,
                    "note": "Timings include detection+recognition, cold start, no warmup; not acceptance metrics.",
                    "files": {str(p.resolve()): hashlib.sha256(p.read_bytes()).hexdigest() for p in
                              assets}}
        with args.output.with_suffix(".run.json").open("x", encoding="utf-8", newline="\n") as file:
            json.dump(metadata, file, ensure_ascii=False, indent=2)
    finally:
        ocr.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for flag in ("capture", "output", "plugin"):
        parser.add_argument("--" + flag, type=Path, required=True)
    for flag in ("profile", "det-model", "rec-model", "dictionary"):
        parser.add_argument("--" + flag, type=Path)
    args = parser.parse_args()
    if bool(args.profile) == bool(args.det_model or args.rec_model or args.dictionary):
        parser.error("Choose --profile OR all three explicit model/dictionary paths")
    if not args.profile and not all((args.det_model, args.rec_model, args.dictionary)):
        parser.error("Explicit baseline requires det-model, rec-model and dictionary")
    try:
        prelabel(args)
    except (ValueError, RuntimeError, OSError) as exc:
        parser.exit(2, f"error: {exc}\n")
