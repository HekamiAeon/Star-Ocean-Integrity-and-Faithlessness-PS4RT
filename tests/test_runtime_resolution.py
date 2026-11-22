from paths import ROOT
import subprocess
import unittest

EXE=ROOT/'out/runtime-resolution-test'

class RuntimeResolutionTests(unittest.TestCase):
    def test_internal_resolution_game_patches(self):
        result=subprocess.run([EXE],capture_output=True,text=True,timeout=120)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        self.assertIn('PASS: internal resolution game patches',result.stdout)

if __name__=='__main__':
    unittest.main()
