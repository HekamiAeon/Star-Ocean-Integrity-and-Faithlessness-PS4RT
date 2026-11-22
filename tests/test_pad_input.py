from paths import ROOT
import subprocess
import unittest

EXE=ROOT/'out/pad-input-test'

class PadInputTests(unittest.TestCase):
    def test_stick_dead_zone(self):
        result=subprocess.run([EXE],capture_output=True,text=True,timeout=120)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        self.assertIn('PASS: stick dead zone',result.stdout)

if __name__=='__main__':
    unittest.main()
