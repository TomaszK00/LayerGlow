"""Check signer key matching, detached signature, and protected route coverage."""
import base64
import json
import pathlib
import subprocess
import tempfile
import unittest

from importlib.util import module_from_spec, spec_from_file_location

ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE = (ROOT / 'PlatformIO/src/main.cpp').read_text()
SIGNER = ROOT / 'security/sign_ota.py'


class SecurityTests(unittest.TestCase):
    def test_pinned_public_key_and_route_guards(self):
        self.assertIn((ROOT / 'security/ota_public.pem').read_text().strip(), SOURCE)
        for route in ('/api/status', '/api/network/info', '/api/printers/saved',
                      '/api/printers/discovery', '/api/wifi/scan', '/api/ota/capacity'):
            self.assertIn('server.on("' + route + '",HTTP_GET,[](){', SOURCE)
        self.assertIn('if(!WiFi.softAP(AP_NAME))', SOURCE)
        self.assertNotIn('/api/security/enroll', SOURCE)
        self.assertNotIn('WWW-Authenticate', SOURCE)
        self.assertNotIn('INITIAL_AP_PASSWORD', SOURCE)
        self.assertIn('if(!local||(!origin.isEmpty()&&origin!="http://"+host))', SOURCE)
        self.assertIn('if(Update.end(true))otaImageReady=true;', SOURCE)
        self.assertIn('verifyOtaSignature(digest)', SOURCE)

    def test_signer_rejects_wrong_key_and_tampering(self):
        spec = spec_from_file_location('layerglow_signer', SIGNER)
        module = module_from_spec(spec);spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory() as folder:
            path = pathlib.Path(folder);key=path / 'private.pem';public=path / 'public.pem'
            other=path / 'other.pem';image=path / 'firmware.bin';software=path / 'Software'
            def run(*args): return subprocess.run(['openssl', *args], check=True, capture_output=True)
            run('genpkey','-algorithm','RSA','-pkeyopt','rsa_keygen_bits:2048','-out',str(key))
            public.write_bytes(run('pkey','-in',str(key),'-pubout').stdout)
            run('genpkey','-algorithm','RSA','-pkeyopt','rsa_keygen_bits:2048','-out',str(other))
            image.write_bytes(b'\xe9' + bytes(120_000))
            with self.assertRaises(ValueError): module.sign(image, other, '2.52', software, public)
            manifest=module.sign(image,key,'2.52',software,public)
            self.assertEqual(json.loads((software/'update.json').read_text()),manifest)
            sig=path/'sig.bin';sig.write_bytes(base64.b64decode(manifest['signature']))
            signed=software/manifest['file']
            run('dgst','-sha256','-verify',str(public),'-signature',str(sig),str(signed))
            signed.write_bytes(signed.read_bytes()[:-1]+b'X')
            failed=subprocess.run(['openssl','dgst','-sha256','-verify',str(public),'-signature',str(sig),str(signed)],capture_output=True)
            self.assertNotEqual(failed.returncode,0)


if __name__=='__main__': unittest.main()
