import os
import torch
import torch.nn as nn
import torchvision.models as models

class PersonAttributeNet(nn.Module):
    def __init__(self):
        super().__init__()
        backbone = models.mobilenet_v3_small()
        self.features = backbone.features
        self.avgpool = nn.AdaptiveAvgPool2d((1, 1))
        # mobilenet_v3_small output feature dim is 576
        self.fc = nn.Sequential(
            nn.Linear(576, 256),
            nn.Hardswish(),
            nn.Dropout(p=0.2)
        )
        # Head 1: Gender logits (2 classes: [0]=男, [1]=女)
        self.gender_head = nn.Linear(256, 2)
        # Head 2: Age group logits (4 classes: [0]=少年(<18), [1]=青年(18-35), [2]=中年(36-55), [3]=老年(>55))
        self.age_head = nn.Linear(256, 4)

    def forward(self, x):
        feat = self.features(x)
        feat = self.avgpool(feat)
        feat = torch.flatten(feat, 1)
        feat = self.fc(feat)
        gender_logits = self.gender_head(feat)
        age_logits = self.age_head(feat)
        return gender_logits, age_logits

def export():
    os.makedirs("models", exist_ok=True)
    out_path = "models/mobilenetv3_attributes.onnx"
    model = PersonAttributeNet()
    model.eval()

    dummy_input = torch.randn(1, 3, 224, 224, dtype=torch.float32)
    torch.onnx.export(
        model,
        dummy_input,
        out_path,
        input_names=["input"],
        output_names=["gender_logits", "age_logits"],
        dynamic_axes={
            "input": {0: "batch_size"},
            "gender_logits": {0: "batch_size"},
            "age_logits": {0: "batch_size"}
        },
        opset_version=12,
        dynamo=False
    )
    size_mb = os.path.getsize(out_path) / (1024 * 1024)
    print(f"Successfully exported {out_path}, size: {size_mb:.2f} MB")

if __name__ == "__main__":
    export()
