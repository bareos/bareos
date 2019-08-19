// //////////////////////////////////////////////////////////
// crc32.js
// Copyright (c) 2014 Stephan Brumme. All rights reserved.
// see http://create.stephan-brumme.com/disclaimer.html
//

function hex(what)
{
  // adjust negative numbers
  if (what < 0)
    what = 0xFFFFFFFF + what + 1;
  // convert to hexadecimal string
  var result = what.toString(16);
  // add leading zeros
  return ('0000000' + result).slice(-8);
}

function crc32_bitwise(text)
{
  // CRC32b polynomial
  var Polynomial = 0xEDB88320;
  // start value
  var crc = 0xFFFFFFFF;

  for (var i = 0; i < text.length; i++)
  {
    // XOR next byte into state
    crc ^= text.charCodeAt(i);

    // process 8 bits
    for (var bit = 0; bit < 8; bit++)
    {
      // look at lowest bit
      if ((crc & 1) != 0)
        crc = (crc >>> 1) ^ Polynomial;
      else
        crc =  crc >>> 1;
    }
  }

  // return hex string
  return hex(~crc);
}
