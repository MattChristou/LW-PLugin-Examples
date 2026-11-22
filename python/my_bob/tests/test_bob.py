import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)                      # fake_lwsdk
sys.path.insert(0, os.path.join(os.path.dirname(HERE), "plugin"))

from fake_lwsdk import FakeMotionAccess, FakeState   # installs the fake first
import my_bob


class BobTests(unittest.TestCase):
    def test_peak_at_quarter_period(self):
        bob = my_bob.Bob(context=None)
        ma = FakeMotionAccess(time=0.5, position=(0.0, 1.0, 0.0))
        bob.evaluate(ma)
        self.assertAlmostEqual(ma.position[1], 1.25)

    def test_save_load_round_trip(self):
        src = my_bob.Bob(None)
        src.height, src.period = 0.5, 3.0
        state = FakeState()
        src.inst_save(state)
        dst = my_bob.Bob(None)
        dst.inst_load(state)
        self.assertEqual((dst.height, dst.period), (0.5, 3.0))

    def test_registers_one_server(self):
        self.assertEqual(len(my_bob.ServerRecord), 1)


if __name__ == "__main__":
    unittest.main()
