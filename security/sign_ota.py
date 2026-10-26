"""Sign one LayerGlow ESP32-C3 OTA image; keep the private PEM outside git."""
import argparse
import base64
import hashlib
import json
import subprocess
import tempfile
from pathlib import Path


def sign(image: Path, key: Path, version: str, software: Path, public_key: Path | None = None) -> dict:
    image_bytes = image.read_bytes()
    if not 100_000 < len(image_bytes) <= 1_900_000 or image_bytes[0] != 0xE9:
        raise ValueError("Invalid or oversized ESP32-C3 OTA image")
    if not version.replace('.', '').isdigit() or version.count('.') != 1:
        raise ValueError("Invalid firmware version")
    public = public_key or Path(__file__).with_name('ota_public.pem')
    extracted = subprocess.run(
        ['openssl', 'pkey', '-in', str(key), '-pubout'],
        check=True, capture_output=True,
    ).stdout
    if extracted != public.read_bytes():
        raise ValueError("Signing key does not match the public key in firmware")
    with tempfile.TemporaryDirectory() as folder:
        signature_path = Path(folder) / 'signature.bin'
        subprocess.run(
            ['openssl', 'dgst', '-sha256', '-sign', str(key), '-out', str(signature_path), str(image)],
            check=True,
        )
        subprocess.run(
            ['openssl', 'dgst', '-sha256', '-verify', str(public),
             '-signature', str(signature_path), str(image)],
            check=True, capture_output=True,
        )
        signature = signature_path.read_bytes()
    if len(signature) != 256:
        raise ValueError("Expected an RSA-2048 signature")
    name = f'LayerGlow_v{version.replace(".", "_")}_OTA.bin'
    software.mkdir(exist_ok=True)
    (software / name).write_bytes(image_bytes)
    encoded = base64.b64encode(signature).decode('ascii')
    (software / f'{name}.sig').write_text(encoded + '\n')
    manifest = {
        'version': version, 'file': name, 'size': len(image_bytes),
        'sha256': hashlib.sha256(image_bytes).hexdigest(), 'signature': encoded,
    }
    (software / 'update.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return manifest


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image', required=True, type=Path)
    parser.add_argument('--key', required=True, type=Path)
    parser.add_argument('--version', required=True)
    parser.add_argument('--software', default=Path('Software'), type=Path)
    args = parser.parse_args()
    print(json.dumps(sign(args.image, args.key, args.version, args.software), indent=2))
