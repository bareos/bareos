import unittest
import bareossd
import time


class TestBareosFd(unittest.TestCase):

    def test_GetValue(self):
        bareossd.GetValue()

    def test_SetValue(self):
        bareossd.SetValue()

    def test_DebugMessage(self):
        bareossd.DebugMessage()

    def test_JobMessage(self):
        bareossd.JobMessage()

    def test_RegisterEvents(self):
        bareossd.RegisterEvents()

    def test_UnRegisterEvents(self):
        bareossd.UnRegisterEvents()

    def test_GetInstanceCount(self):
        bareossd.GetInstanceCount()


if __name__ == "__main__":
    unittest.main()
