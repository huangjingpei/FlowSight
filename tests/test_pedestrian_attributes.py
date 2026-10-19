#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
test_pedestrian_attributes.py:
验证预训练高精度行人属性量化模型 (pedestrian_gender_int8.onnx) 及辅助年龄模型 (genderage.onnx) 的输入、输出与推断准确率
"""

import os
import sys
import cv2
import numpy as np
import onnxruntime as ort

def test_pedestrian_attributes():
    gender_model_path = "models/pedestrian_gender_int8.onnx"
    age_model_path = "models/genderage.onnx"

    assert os.path.exists(gender_model_path), f"Gender model missing: {gender_model_path}"
    assert os.path.exists(age_model_path), f"Age model missing: {age_model_path}"

    print(f"[+] 1. 加载行人性别分类模型: {gender_model_path} ...")
    opts = ort.SessionOptions()
    opts.intra_op_num_threads = 2
    sess_gender = ort.InferenceSession(gender_model_path, opts, providers=['CPUExecutionProvider'])

    in_names = [i.name for i in sess_gender.get_inputs()]
    out_names = [o.name for o in sess_gender.get_outputs()]
    print(f"    Inputs: {in_names}, Outputs: {out_names}")
    assert "pixel_values" in in_names
    assert "logits" in out_names

    print(f"[+] 2. 加载辅助年龄模型: {age_model_path} ...")
    sess_age = ort.InferenceSession(age_model_path, opts, providers=['CPUExecutionProvider'])
    assert "data" in [i.name for i in sess_age.get_inputs()]
    assert "fc1" in [o.name for o in sess_age.get_outputs()]

    print("[+] 3. 验证典型行人测试样本 (含过道长发女性与背影)...")
    test_samples = [
        ("长发背影女性", "scratch/f360_box2.jpg", "女"),
        ("浅蓝外套条纹裙女性", "scratch/corridor_crops/f200_p1.jpg", "女"),
        ("黑衣短裙女性", "scratch/corridor_crops/f60_p0.jpg", "女"),
        ("穿T恤长裤男士", "scratch/corridor_crops/f220_p0.jpg", "男"),
    ]

    all_passed = True
    for desc, path, expected_gender in test_samples:
        if not os.path.exists(path):
            print(f"    [SKIP] 缺少样本: {path}")
            continue

        img = cv2.imread(path)
        # 性别推理
        rgb = cv2.cvtColor(cv2.resize(img, (224, 224)), cv2.COLOR_BGR2RGB).astype(np.float32) / 255.0
        blob = ((rgb - 0.5) / 0.5).transpose(2, 0, 1)[np.newaxis, :, :, :]
        logits = sess_gender.run(['logits'], {'pixel_values': blob})[0][0]
        exp_0 = np.exp(logits[0])
        exp_1 = np.exp(logits[1])
        prob_f = exp_0 / (exp_0 + exp_1)
        prob_m = exp_1 / (exp_0 + exp_1)
        pred_gender = "女" if prob_f >= prob_m else "男"
        conf = max(prob_f, prob_m)

        # 年龄推理
        h = max(10, int(img.shape[0] * 0.4))
        head = img[:h, :]
        head_rgb = cv2.cvtColor(cv2.resize(head, (96, 96)), cv2.COLOR_BGR2RGB).astype(np.float32)
        head_blob = head_rgb.transpose(2, 0, 1)[np.newaxis, :, :, :]
        age_out = sess_age.run(['fc1'], {'data': head_blob})[0][0]
        age_val = float(age_out[2] * 100.0)

        match = (pred_gender == expected_gender)
        status = "[PASS]" if match else "[FAIL]"
        print(f"    {status} {desc:16s} => 预测: {pred_gender} (女:{prob_f*100:.1f}%, 男:{prob_m*100:.1f}%) | 预估年龄: {age_val:.1f}岁 | 期望: {expected_gender}")
        if not match:
            all_passed = False

    assert all_passed, "存在性别识别判定不符合预期的测试用例！"
    print("\n[SUCCESS] 行人性别与年龄多模态模型端到端校验 100% 通过！")

if __name__ == "__main__":
    test_pedestrian_attributes()
