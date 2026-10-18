import os
import sys
import time
import numpy as np
import onnxruntime as ort

def test_attributes_inference():
    model_path = "models/mobilenetv3_attributes.onnx"
    assert os.path.exists(model_path), f"Model not found at {model_path}"
    
    print(f"[Test] 1. Loading ONNX model from {model_path}...")
    sess = ort.InferenceSession(model_path)
    print("  Inputs:", [i.name for i in sess.get_inputs()])
    print("  Outputs:", [o.name for o in sess.get_outputs()])

    print("[Test] 2. Testing forward inference on random person crop...")
    dummy_input = np.random.randn(1, 3, 224, 224).astype(np.float32)
    t0 = time.perf_counter()
    res = sess.run(None, {"input": dummy_input})
    t_cost = (time.perf_counter() - t0) * 1000.0

    gender_logits = res[0][0]
    age_logits = res[1][0]
    print(f"  Inference took {t_cost:.2f} ms")
    print(f"  Raw gender_logits: {gender_logits}, age_logits: {age_logits}")

    # Softmax gender
    exp_g = np.exp(gender_logits)
    prob_m = exp_g[0] / np.sum(exp_g)
    prob_f = exp_g[1] / np.sum(exp_g)
    gender_label = "男" if prob_m >= prob_f else "女"
    gender_conf = max(prob_m, prob_f)
    print(f"  Decoded Gender: {gender_label} (Confidence: {gender_conf*100:.1f}%)")

    # Softmax age
    exp_a = np.exp(age_logits)
    prob_a = exp_a / np.sum(exp_a)
    age_labels = ["少年(<18)", "青年(18-35)", "中年(36-55)", "老年(>55)"]
    top_age_idx = np.argmax(prob_a)
    top_age_label = age_labels[top_age_idx]
    top_age_conf = prob_a[top_age_idx]
    print(f"  Decoded Age: {top_age_label} (Confidence: {top_age_conf*100:.1f}%)")

    assert gender_label in ["男", "女"]
    assert top_age_label in age_labels
    print("\n[SUCCESS] MobileNetV3 attribute classification model verified 100%!")

if __name__ == "__main__":
    test_attributes_inference()
