#!/usr/bin/python

class BareosBase64:
    '''
    Bacula and therefore Bareos specific implementation of a base64 decoder
    '''
    base64_digits = \
        ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
         'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
         'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
         'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
         '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/']

    def __init__(self):
        '''
        Initialize the Base 64 conversion routines
        '''
        self.base64_map = dict(zip(self.base64_digits, xrange(0, 64)))

    def twos_comp(self, val, bits):
        """compute the 2's compliment of int value val"""
        if( (val&(1<<(bits-1))) != 0 ):
            val = val - (1<<bits)
        return val

    def decode(self, base64):
        '''
        Convert the Base 64 characters in base64 to a value.
        '''
        value = 0
        first = 0
        neg = False

        if base64[0] == '-':
            neg = True
            first = 1

        for i in xrange(first, len(base64)):
            value = value << 6
            try:
                value += self.base64_map[base64[i]]
            except KeyError as e:
                print "KeyError:", i

        return -value if neg else value

    def to_base64(self, value):
        result = ""
        if value < 0:
            result = "-"
            value=-value

        while value:
            charnumber = value % 0x3F
            result += self.base64_digits[charnumber]
            value = value >> 6
        return result;

    def bin_to_base64(self, binary, compatible = False):
        buf = ""
        j = 0
        reg = 0
        rem = 0
        char = 0
        i = 0
        while i < len(binary):
            if rem < 6:
                reg <<= 8
                char = binary[i]
                if not compatible:
                    if char >= 128:
                        char = self.twos_comp(char,8)
                reg |= char
                i+=1
                rem += 8

            save = reg
            reg >>= (rem - 6)
            buf += self.base64_digits[reg & 0x3F];
            j+=1
            reg = save;
            rem -= 6;

        if rem:
            mask = (1 << rem) - 1;
            if (compatible):
                buf += self.base64_digits[(reg & mask) << (6 - rem)];
            else:
                buf += self.base64_digits[reg & mask];
            j+=1
        return buf
