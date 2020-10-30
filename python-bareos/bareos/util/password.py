"""
Class to handle Bareos passwords.
"""

import hashlib


class Password(object):
    def __init__(self, password=None):
        self.password_md5 = None
        self.set_plaintext(password)

    def set_plaintext(self, password):
        self.password_plaintext = bytearray(password, "utf-8")
        self.set_md5(self.__plaintext2md5(password))

    def set_md5(self, password):
        self.password_md5 = password

    def plaintext(self):
        return self.password_plaintext

    def md5(self):
        return self.password_md5

    @staticmethod
    def __plaintext2md5(password):
        """
        md5 the password and return the hex style
        """
        md5 = hashlib.md5()
        md5.update(bytes(bytearray(password, "utf-8")))
        return bytes(bytearray(md5.hexdigest(), "utf-8"))
