import unittest
import bareossd
import time

print help (bareossd)

class TestBareosFd(unittest.TestCase):

    def test_GetValue(self):
        self.assertRaises(TypeError, bareossd.GetValue)
        self.assertRaises(RuntimeError, bareossd.GetValue, 1)

    # def test_SetValue(self):
    #     self.assertRaises(TypeError, bareossd.SetValue)
    #     self.assertRaises(RuntimeError, bareossd.SetValue, 2)

    def test_DebugMessage(self):
        self.assertRaises(TypeError, bareossd.DebugMessage, "This is a Debug message")
        self.assertRaises(RuntimeError, bareossd.DebugMessage, 100, "This is a Debug message")

    def test_JobMessage(self):
        self.assertRaises(TypeError, bareossd.JobMessage, "This is a Job message")
        self.assertRaises(RuntimeError, bareossd.JobMessage, 100, "This is a Job message")

    # def test_RegisterEvents(self):
    #     self.assertRaises(RuntimeError, bareossd.RegisterEvents)

    # def test_UnRegisterEvents(self):
    #     self.assertRaises(RuntimeError, bareossd.UnRegisterEvents)

    def test_GetInstanceCount(self):
        self.assertRaises(RuntimeError, bareossd.GetInstanceCount)


if __name__ == "__main__":
    unittest.main()
