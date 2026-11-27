from paths import ROOT
import subprocess
import unittest

EXE=ROOT/'out/runtime-quality-test'

class RuntimeQualityTests(unittest.TestCase):
    def test_graphics_quality_game_patches(self):
        result=subprocess.run([EXE],capture_output=True,text=True,timeout=120)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        self.assertIn('PASS: graphics quality game patches',result.stdout)

if __name__=='__main__':
    unittest.main()
