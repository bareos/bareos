import unittest
import bareosdir
import time


class TestBareosFd(unittest.TestCase):

    def test_GetValue(self):
        bareosdir.GetValue()

    def test_SetValue(self):
        bareosdir.SetValue()

    def test_DebugMessage(self):
        bareosdir.DebugMessage()

    def test_JobMessage(self):
        bareosdir.JobMessage()

    def test_RegisterEvents(self):
        bareosdir.RegisterEvents()

    def test_UnRegisterEvents(self):
        bareosdir.UnRegisterEvents()

    def test_GetInstanceCount(self):
        bareosdir.GetInstanceCount()


if __name__ == "__main__":
    unittest.main()
