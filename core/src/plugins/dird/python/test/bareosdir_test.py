import unittest
import bareosdir
import time

print help(bareosdir)

class TestBareosFd(unittest.TestCase):

    def test_GetValue(self):
        self.assertRaises(RuntimeError, bareosdir.GetValue, 1)
        self.assertRaises(RuntimeError, bareosdir.GetValue, 1)

    def test_SetValue(self):
        bareosdir.SetValue()
        # self.assertRaises(RuntimeError, bareosdir.SetValue)
        # self.assertRaises(RuntimeError, bareosdir.SetValue, 2)

    def test_DebugMessage(self):
        self.assertRaises(TypeError, bareosdir.DebugMessage, "This is a debug message")
        self.assertRaises(RuntimeError, bareosdir.DebugMessage,100, "This is a debug message")

    def test_JobMessage(self):
        self.assertRaises(TypeError, bareosdir.JobMessage, "This is a Job message")
        self.assertRaises(RuntimeError, bareosdir.JobMessage,100, "This is a Job message")

    def test_RegisterEvents(self):
        # self.assertRaises(TypeError, bareosdir.RegisterEvents)
        self.assertRaises(RuntimeError, bareosdir.RegisterEvents, 1)

    def test_UnRegisterEvents(self):
        self.assertRaises(RuntimeError, bareosdir.UnRegisterEvents, 1)

    def test_GetInstanceCount(self):
        self.assertRaises(RuntimeError, bareosdir.GetInstanceCount)


if __name__ == "__main__":
    unittest.main()
