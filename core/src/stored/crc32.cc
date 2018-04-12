/*
   crc32.c 32 bit CRC

   Copyright (C) 2010 Joakim Tjernlund
   Copyright (C) 2016-2016 Bareos GmbH & Co. KG

   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser Affero General Public License for more details.
*/
/*
 * By Kern Sibbald, January 2001
 * Improved, faster version
 * By Joakim Tjernlunc, 2010
 */
/**
 * @file
 * Original 32 bit CRC.  Algorithm from RFC 2083 (png format)
 */

#ifdef GENERATE_STATIC_CRC_TABLE
/**
 * The following code can be used to generate the static CRC table.
 *
 * Note, the magic number 0xedb88320L below comes from the terms
 * of the defining polynomial x^n,
 * where n=0,1,2,4,5,7,8,10,11,12,16,22,23,26
 */
#include <stdio.h>

main()
{
   unsigned long crc;
   unsigned long buf[5];
   int i, j, k;
   k = 0;
   for (i = 0; i < 256; i++) {
      crc = (unsigned long)i;
      for (j = 0; j < 8; j++) {
         if (crc & 1) {
            crc = 0xedb88320L ^ (crc >> 1);
         } else {
            crc = crc >> 1;
         }
      }
      buf[k++] = crc;
      if (k == 5) {
         k = 0;
         printf("  0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x,\n",
            buf[0], buf[1], buf[2], buf[3], buf[4]);
      }
   }
   printf("  0x%08x\n", buf[0]);
}
#endif

#include "bareos.h"

#if defined(AC_APPLE_UNIVERSAL_BUILD)
#error This code only supports compile time endianess selection not during runtime.
#endif

#if !defined(HAVE_LITTLE_ENDIAN) && !defined(HAVE_BIG_ENDIAN)
#error Either HAVE_LITTLE_ENDIAN or HAVE_BIG_ENDIAN must be defined!
#endif

/* tole == To Little Endian */
#ifdef HAVE_BIG_ENDIAN
#define tole(x) \
       ((uint32_t)(                                               \
               (((uint32_t)(x) & (uint32_t)0x000000ffUL) << 24) | \
               (((uint32_t)(x) & (uint32_t)0x0000ff00UL) <<  8) | \
               (((uint32_t)(x) & (uint32_t)0x00ff0000UL) >>  8) | \
               (((uint32_t)(x) & (uint32_t)0xff000000UL) >> 24) ))

#else
#define tole(x) x
#endif

/**
 * The magic number 0xedb88320L below comes from the terms
 * of the defining polynomial x^n,
 * where n=0,1,2,4,5,7,8,10,11,12,16,22,23,26
 */

static const uint32_t tab[4][256] = {{
tole(0x00000000L), tole(0x77073096L), tole(0xee0e612cL), tole(0x990951baL),
tole(0x076dc419L), tole(0x706af48fL), tole(0xe963a535L), tole(0x9e6495a3L),
tole(0x0edb8832L), tole(0x79dcb8a4L), tole(0xe0d5e91eL), tole(0x97d2d988L),
tole(0x09b64c2bL), tole(0x7eb17cbdL), tole(0xe7b82d07L), tole(0x90bf1d91L),
tole(0x1db71064L), tole(0x6ab020f2L), tole(0xf3b97148L), tole(0x84be41deL),
tole(0x1adad47dL), tole(0x6ddde4ebL), tole(0xf4d4b551L), tole(0x83d385c7L),
tole(0x136c9856L), tole(0x646ba8c0L), tole(0xfd62f97aL), tole(0x8a65c9ecL),
tole(0x14015c4fL), tole(0x63066cd9L), tole(0xfa0f3d63L), tole(0x8d080df5L),
tole(0x3b6e20c8L), tole(0x4c69105eL), tole(0xd56041e4L), tole(0xa2677172L),
tole(0x3c03e4d1L), tole(0x4b04d447L), tole(0xd20d85fdL), tole(0xa50ab56bL),
tole(0x35b5a8faL), tole(0x42b2986cL), tole(0xdbbbc9d6L), tole(0xacbcf940L),
tole(0x32d86ce3L), tole(0x45df5c75L), tole(0xdcd60dcfL), tole(0xabd13d59L),
tole(0x26d930acL), tole(0x51de003aL), tole(0xc8d75180L), tole(0xbfd06116L),
tole(0x21b4f4b5L), tole(0x56b3c423L), tole(0xcfba9599L), tole(0xb8bda50fL),
tole(0x2802b89eL), tole(0x5f058808L), tole(0xc60cd9b2L), tole(0xb10be924L),
tole(0x2f6f7c87L), tole(0x58684c11L), tole(0xc1611dabL), tole(0xb6662d3dL),
tole(0x76dc4190L), tole(0x01db7106L), tole(0x98d220bcL), tole(0xefd5102aL),
tole(0x71b18589L), tole(0x06b6b51fL), tole(0x9fbfe4a5L), tole(0xe8b8d433L),
tole(0x7807c9a2L), tole(0x0f00f934L), tole(0x9609a88eL), tole(0xe10e9818L),
tole(0x7f6a0dbbL), tole(0x086d3d2dL), tole(0x91646c97L), tole(0xe6635c01L),
tole(0x6b6b51f4L), tole(0x1c6c6162L), tole(0x856530d8L), tole(0xf262004eL),
tole(0x6c0695edL), tole(0x1b01a57bL), tole(0x8208f4c1L), tole(0xf50fc457L),
tole(0x65b0d9c6L), tole(0x12b7e950L), tole(0x8bbeb8eaL), tole(0xfcb9887cL),
tole(0x62dd1ddfL), tole(0x15da2d49L), tole(0x8cd37cf3L), tole(0xfbd44c65L),
tole(0x4db26158L), tole(0x3ab551ceL), tole(0xa3bc0074L), tole(0xd4bb30e2L),
tole(0x4adfa541L), tole(0x3dd895d7L), tole(0xa4d1c46dL), tole(0xd3d6f4fbL),
tole(0x4369e96aL), tole(0x346ed9fcL), tole(0xad678846L), tole(0xda60b8d0L),
tole(0x44042d73L), tole(0x33031de5L), tole(0xaa0a4c5fL), tole(0xdd0d7cc9L),
tole(0x5005713cL), tole(0x270241aaL), tole(0xbe0b1010L), tole(0xc90c2086L),
tole(0x5768b525L), tole(0x206f85b3L), tole(0xb966d409L), tole(0xce61e49fL),
tole(0x5edef90eL), tole(0x29d9c998L), tole(0xb0d09822L), tole(0xc7d7a8b4L),
tole(0x59b33d17L), tole(0x2eb40d81L), tole(0xb7bd5c3bL), tole(0xc0ba6cadL),
tole(0xedb88320L), tole(0x9abfb3b6L), tole(0x03b6e20cL), tole(0x74b1d29aL),
tole(0xead54739L), tole(0x9dd277afL), tole(0x04db2615L), tole(0x73dc1683L),
tole(0xe3630b12L), tole(0x94643b84L), tole(0x0d6d6a3eL), tole(0x7a6a5aa8L),
tole(0xe40ecf0bL), tole(0x9309ff9dL), tole(0x0a00ae27L), tole(0x7d079eb1L),
tole(0xf00f9344L), tole(0x8708a3d2L), tole(0x1e01f268L), tole(0x6906c2feL),
tole(0xf762575dL), tole(0x806567cbL), tole(0x196c3671L), tole(0x6e6b06e7L),
tole(0xfed41b76L), tole(0x89d32be0L), tole(0x10da7a5aL), tole(0x67dd4accL),
tole(0xf9b9df6fL), tole(0x8ebeeff9L), tole(0x17b7be43L), tole(0x60b08ed5L),
tole(0xd6d6a3e8L), tole(0xa1d1937eL), tole(0x38d8c2c4L), tole(0x4fdff252L),
tole(0xd1bb67f1L), tole(0xa6bc5767L), tole(0x3fb506ddL), tole(0x48b2364bL),
tole(0xd80d2bdaL), tole(0xaf0a1b4cL), tole(0x36034af6L), tole(0x41047a60L),
tole(0xdf60efc3L), tole(0xa867df55L), tole(0x316e8eefL), tole(0x4669be79L),
tole(0xcb61b38cL), tole(0xbc66831aL), tole(0x256fd2a0L), tole(0x5268e236L),
tole(0xcc0c7795L), tole(0xbb0b4703L), tole(0x220216b9L), tole(0x5505262fL),
tole(0xc5ba3bbeL), tole(0xb2bd0b28L), tole(0x2bb45a92L), tole(0x5cb36a04L),
tole(0xc2d7ffa7L), tole(0xb5d0cf31L), tole(0x2cd99e8bL), tole(0x5bdeae1dL),
tole(0x9b64c2b0L), tole(0xec63f226L), tole(0x756aa39cL), tole(0x026d930aL),
tole(0x9c0906a9L), tole(0xeb0e363fL), tole(0x72076785L), tole(0x05005713L),
tole(0x95bf4a82L), tole(0xe2b87a14L), tole(0x7bb12baeL), tole(0x0cb61b38L),
tole(0x92d28e9bL), tole(0xe5d5be0dL), tole(0x7cdcefb7L), tole(0x0bdbdf21L),
tole(0x86d3d2d4L), tole(0xf1d4e242L), tole(0x68ddb3f8L), tole(0x1fda836eL),
tole(0x81be16cdL), tole(0xf6b9265bL), tole(0x6fb077e1L), tole(0x18b74777L),
tole(0x88085ae6L), tole(0xff0f6a70L), tole(0x66063bcaL), tole(0x11010b5cL),
tole(0x8f659effL), tole(0xf862ae69L), tole(0x616bffd3L), tole(0x166ccf45L),
tole(0xa00ae278L), tole(0xd70dd2eeL), tole(0x4e048354L), tole(0x3903b3c2L),
tole(0xa7672661L), tole(0xd06016f7L), tole(0x4969474dL), tole(0x3e6e77dbL),
tole(0xaed16a4aL), tole(0xd9d65adcL), tole(0x40df0b66L), tole(0x37d83bf0L),
tole(0xa9bcae53L), tole(0xdebb9ec5L), tole(0x47b2cf7fL), tole(0x30b5ffe9L),
tole(0xbdbdf21cL), tole(0xcabac28aL), tole(0x53b39330L), tole(0x24b4a3a6L),
tole(0xbad03605L), tole(0xcdd70693L), tole(0x54de5729L), tole(0x23d967bfL),
tole(0xb3667a2eL), tole(0xc4614ab8L), tole(0x5d681b02L), tole(0x2a6f2b94L),
tole(0xb40bbe37L), tole(0xc30c8ea1L), tole(0x5a05df1bL), tole(0x2d02ef8dL)},
{
tole(0x00000000L), tole(0x191b3141L), tole(0x32366282L), tole(0x2b2d53c3L),
tole(0x646cc504L), tole(0x7d77f445L), tole(0x565aa786L), tole(0x4f4196c7L),
tole(0xc8d98a08L), tole(0xd1c2bb49L), tole(0xfaefe88aL), tole(0xe3f4d9cbL),
tole(0xacb54f0cL), tole(0xb5ae7e4dL), tole(0x9e832d8eL), tole(0x87981ccfL),
tole(0x4ac21251L), tole(0x53d92310L), tole(0x78f470d3L), tole(0x61ef4192L),
tole(0x2eaed755L), tole(0x37b5e614L), tole(0x1c98b5d7L), tole(0x05838496L),
tole(0x821b9859L), tole(0x9b00a918L), tole(0xb02dfadbL), tole(0xa936cb9aL),
tole(0xe6775d5dL), tole(0xff6c6c1cL), tole(0xd4413fdfL), tole(0xcd5a0e9eL),
tole(0x958424a2L), tole(0x8c9f15e3L), tole(0xa7b24620L), tole(0xbea97761L),
tole(0xf1e8e1a6L), tole(0xe8f3d0e7L), tole(0xc3de8324L), tole(0xdac5b265L),
tole(0x5d5daeaaL), tole(0x44469febL), tole(0x6f6bcc28L), tole(0x7670fd69L),
tole(0x39316baeL), tole(0x202a5aefL), tole(0x0b07092cL), tole(0x121c386dL),
tole(0xdf4636f3L), tole(0xc65d07b2L), tole(0xed705471L), tole(0xf46b6530L),
tole(0xbb2af3f7L), tole(0xa231c2b6L), tole(0x891c9175L), tole(0x9007a034L),
tole(0x179fbcfbL), tole(0x0e848dbaL), tole(0x25a9de79L), tole(0x3cb2ef38L),
tole(0x73f379ffL), tole(0x6ae848beL), tole(0x41c51b7dL), tole(0x58de2a3cL),
tole(0xf0794f05L), tole(0xe9627e44L), tole(0xc24f2d87L), tole(0xdb541cc6L),
tole(0x94158a01L), tole(0x8d0ebb40L), tole(0xa623e883L), tole(0xbf38d9c2L),
tole(0x38a0c50dL), tole(0x21bbf44cL), tole(0x0a96a78fL), tole(0x138d96ceL),
tole(0x5ccc0009L), tole(0x45d73148L), tole(0x6efa628bL), tole(0x77e153caL),
tole(0xbabb5d54L), tole(0xa3a06c15L), tole(0x888d3fd6L), tole(0x91960e97L),
tole(0xded79850L), tole(0xc7cca911L), tole(0xece1fad2L), tole(0xf5facb93L),
tole(0x7262d75cL), tole(0x6b79e61dL), tole(0x4054b5deL), tole(0x594f849fL),
tole(0x160e1258L), tole(0x0f152319L), tole(0x243870daL), tole(0x3d23419bL),
tole(0x65fd6ba7L), tole(0x7ce65ae6L), tole(0x57cb0925L), tole(0x4ed03864L),
tole(0x0191aea3L), tole(0x188a9fe2L), tole(0x33a7cc21L), tole(0x2abcfd60L),
tole(0xad24e1afL), tole(0xb43fd0eeL), tole(0x9f12832dL), tole(0x8609b26cL),
tole(0xc94824abL), tole(0xd05315eaL), tole(0xfb7e4629L), tole(0xe2657768L),
tole(0x2f3f79f6L), tole(0x362448b7L), tole(0x1d091b74L), tole(0x04122a35L),
tole(0x4b53bcf2L), tole(0x52488db3L), tole(0x7965de70L), tole(0x607eef31L),
tole(0xe7e6f3feL), tole(0xfefdc2bfL), tole(0xd5d0917cL), tole(0xcccba03dL),
tole(0x838a36faL), tole(0x9a9107bbL), tole(0xb1bc5478L), tole(0xa8a76539L),
tole(0x3b83984bL), tole(0x2298a90aL), tole(0x09b5fac9L), tole(0x10aecb88L),
tole(0x5fef5d4fL), tole(0x46f46c0eL), tole(0x6dd93fcdL), tole(0x74c20e8cL),
tole(0xf35a1243L), tole(0xea412302L), tole(0xc16c70c1L), tole(0xd8774180L),
tole(0x9736d747L), tole(0x8e2de606L), tole(0xa500b5c5L), tole(0xbc1b8484L),
tole(0x71418a1aL), tole(0x685abb5bL), tole(0x4377e898L), tole(0x5a6cd9d9L),
tole(0x152d4f1eL), tole(0x0c367e5fL), tole(0x271b2d9cL), tole(0x3e001cddL),
tole(0xb9980012L), tole(0xa0833153L), tole(0x8bae6290L), tole(0x92b553d1L),
tole(0xddf4c516L), tole(0xc4eff457L), tole(0xefc2a794L), tole(0xf6d996d5L),
tole(0xae07bce9L), tole(0xb71c8da8L), tole(0x9c31de6bL), tole(0x852aef2aL),
tole(0xca6b79edL), tole(0xd37048acL), tole(0xf85d1b6fL), tole(0xe1462a2eL),
tole(0x66de36e1L), tole(0x7fc507a0L), tole(0x54e85463L), tole(0x4df36522L),
tole(0x02b2f3e5L), tole(0x1ba9c2a4L), tole(0x30849167L), tole(0x299fa026L),
tole(0xe4c5aeb8L), tole(0xfdde9ff9L), tole(0xd6f3cc3aL), tole(0xcfe8fd7bL),
tole(0x80a96bbcL), tole(0x99b25afdL), tole(0xb29f093eL), tole(0xab84387fL),
tole(0x2c1c24b0L), tole(0x350715f1L), tole(0x1e2a4632L), tole(0x07317773L),
tole(0x4870e1b4L), tole(0x516bd0f5L), tole(0x7a468336L), tole(0x635db277L),
tole(0xcbfad74eL), tole(0xd2e1e60fL), tole(0xf9ccb5ccL), tole(0xe0d7848dL),
tole(0xaf96124aL), tole(0xb68d230bL), tole(0x9da070c8L), tole(0x84bb4189L),
tole(0x03235d46L), tole(0x1a386c07L), tole(0x31153fc4L), tole(0x280e0e85L),
tole(0x674f9842L), tole(0x7e54a903L), tole(0x5579fac0L), tole(0x4c62cb81L),
tole(0x8138c51fL), tole(0x9823f45eL), tole(0xb30ea79dL), tole(0xaa1596dcL),
tole(0xe554001bL), tole(0xfc4f315aL), tole(0xd7626299L), tole(0xce7953d8L),
tole(0x49e14f17L), tole(0x50fa7e56L), tole(0x7bd72d95L), tole(0x62cc1cd4L),
tole(0x2d8d8a13L), tole(0x3496bb52L), tole(0x1fbbe891L), tole(0x06a0d9d0L),
tole(0x5e7ef3ecL), tole(0x4765c2adL), tole(0x6c48916eL), tole(0x7553a02fL),
tole(0x3a1236e8L), tole(0x230907a9L), tole(0x0824546aL), tole(0x113f652bL),
tole(0x96a779e4L), tole(0x8fbc48a5L), tole(0xa4911b66L), tole(0xbd8a2a27L),
tole(0xf2cbbce0L), tole(0xebd08da1L), tole(0xc0fdde62L), tole(0xd9e6ef23L),
tole(0x14bce1bdL), tole(0x0da7d0fcL), tole(0x268a833fL), tole(0x3f91b27eL),
tole(0x70d024b9L), tole(0x69cb15f8L), tole(0x42e6463bL), tole(0x5bfd777aL),
tole(0xdc656bb5L), tole(0xc57e5af4L), tole(0xee530937L), tole(0xf7483876L),
tole(0xb809aeb1L), tole(0xa1129ff0L), tole(0x8a3fcc33L), tole(0x9324fd72L)},
{
tole(0x00000000L), tole(0x01c26a37L), tole(0x0384d46eL), tole(0x0246be59L),
tole(0x0709a8dcL), tole(0x06cbc2ebL), tole(0x048d7cb2L), tole(0x054f1685L),
tole(0x0e1351b8L), tole(0x0fd13b8fL), tole(0x0d9785d6L), tole(0x0c55efe1L),
tole(0x091af964L), tole(0x08d89353L), tole(0x0a9e2d0aL), tole(0x0b5c473dL),
tole(0x1c26a370L), tole(0x1de4c947L), tole(0x1fa2771eL), tole(0x1e601d29L),
tole(0x1b2f0bacL), tole(0x1aed619bL), tole(0x18abdfc2L), tole(0x1969b5f5L),
tole(0x1235f2c8L), tole(0x13f798ffL), tole(0x11b126a6L), tole(0x10734c91L),
tole(0x153c5a14L), tole(0x14fe3023L), tole(0x16b88e7aL), tole(0x177ae44dL),
tole(0x384d46e0L), tole(0x398f2cd7L), tole(0x3bc9928eL), tole(0x3a0bf8b9L),
tole(0x3f44ee3cL), tole(0x3e86840bL), tole(0x3cc03a52L), tole(0x3d025065L),
tole(0x365e1758L), tole(0x379c7d6fL), tole(0x35dac336L), tole(0x3418a901L),
tole(0x3157bf84L), tole(0x3095d5b3L), tole(0x32d36beaL), tole(0x331101ddL),
tole(0x246be590L), tole(0x25a98fa7L), tole(0x27ef31feL), tole(0x262d5bc9L),
tole(0x23624d4cL), tole(0x22a0277bL), tole(0x20e69922L), tole(0x2124f315L),
tole(0x2a78b428L), tole(0x2bbade1fL), tole(0x29fc6046L), tole(0x283e0a71L),
tole(0x2d711cf4L), tole(0x2cb376c3L), tole(0x2ef5c89aL), tole(0x2f37a2adL),
tole(0x709a8dc0L), tole(0x7158e7f7L), tole(0x731e59aeL), tole(0x72dc3399L),
tole(0x7793251cL), tole(0x76514f2bL), tole(0x7417f172L), tole(0x75d59b45L),
tole(0x7e89dc78L), tole(0x7f4bb64fL), tole(0x7d0d0816L), tole(0x7ccf6221L),
tole(0x798074a4L), tole(0x78421e93L), tole(0x7a04a0caL), tole(0x7bc6cafdL),
tole(0x6cbc2eb0L), tole(0x6d7e4487L), tole(0x6f38fadeL), tole(0x6efa90e9L),
tole(0x6bb5866cL), tole(0x6a77ec5bL), tole(0x68315202L), tole(0x69f33835L),
tole(0x62af7f08L), tole(0x636d153fL), tole(0x612bab66L), tole(0x60e9c151L),
tole(0x65a6d7d4L), tole(0x6464bde3L), tole(0x662203baL), tole(0x67e0698dL),
tole(0x48d7cb20L), tole(0x4915a117L), tole(0x4b531f4eL), tole(0x4a917579L),
tole(0x4fde63fcL), tole(0x4e1c09cbL), tole(0x4c5ab792L), tole(0x4d98dda5L),
tole(0x46c49a98L), tole(0x4706f0afL), tole(0x45404ef6L), tole(0x448224c1L),
tole(0x41cd3244L), tole(0x400f5873L), tole(0x4249e62aL), tole(0x438b8c1dL),
tole(0x54f16850L), tole(0x55330267L), tole(0x5775bc3eL), tole(0x56b7d609L),
tole(0x53f8c08cL), tole(0x523aaabbL), tole(0x507c14e2L), tole(0x51be7ed5L),
tole(0x5ae239e8L), tole(0x5b2053dfL), tole(0x5966ed86L), tole(0x58a487b1L),
tole(0x5deb9134L), tole(0x5c29fb03L), tole(0x5e6f455aL), tole(0x5fad2f6dL),
tole(0xe1351b80L), tole(0xe0f771b7L), tole(0xe2b1cfeeL), tole(0xe373a5d9L),
tole(0xe63cb35cL), tole(0xe7fed96bL), tole(0xe5b86732L), tole(0xe47a0d05L),
tole(0xef264a38L), tole(0xeee4200fL), tole(0xeca29e56L), tole(0xed60f461L),
tole(0xe82fe2e4L), tole(0xe9ed88d3L), tole(0xebab368aL), tole(0xea695cbdL),
tole(0xfd13b8f0L), tole(0xfcd1d2c7L), tole(0xfe976c9eL), tole(0xff5506a9L),
tole(0xfa1a102cL), tole(0xfbd87a1bL), tole(0xf99ec442L), tole(0xf85cae75L),
tole(0xf300e948L), tole(0xf2c2837fL), tole(0xf0843d26L), tole(0xf1465711L),
tole(0xf4094194L), tole(0xf5cb2ba3L), tole(0xf78d95faL), tole(0xf64fffcdL),
tole(0xd9785d60L), tole(0xd8ba3757L), tole(0xdafc890eL), tole(0xdb3ee339L),
tole(0xde71f5bcL), tole(0xdfb39f8bL), tole(0xddf521d2L), tole(0xdc374be5L),
tole(0xd76b0cd8L), tole(0xd6a966efL), tole(0xd4efd8b6L), tole(0xd52db281L),
tole(0xd062a404L), tole(0xd1a0ce33L), tole(0xd3e6706aL), tole(0xd2241a5dL),
tole(0xc55efe10L), tole(0xc49c9427L), tole(0xc6da2a7eL), tole(0xc7184049L),
tole(0xc25756ccL), tole(0xc3953cfbL), tole(0xc1d382a2L), tole(0xc011e895L),
tole(0xcb4dafa8L), tole(0xca8fc59fL), tole(0xc8c97bc6L), tole(0xc90b11f1L),
tole(0xcc440774L), tole(0xcd866d43L), tole(0xcfc0d31aL), tole(0xce02b92dL),
tole(0x91af9640L), tole(0x906dfc77L), tole(0x922b422eL), tole(0x93e92819L),
tole(0x96a63e9cL), tole(0x976454abL), tole(0x9522eaf2L), tole(0x94e080c5L),
tole(0x9fbcc7f8L), tole(0x9e7eadcfL), tole(0x9c381396L), tole(0x9dfa79a1L),
tole(0x98b56f24L), tole(0x99770513L), tole(0x9b31bb4aL), tole(0x9af3d17dL),
tole(0x8d893530L), tole(0x8c4b5f07L), tole(0x8e0de15eL), tole(0x8fcf8b69L),
tole(0x8a809decL), tole(0x8b42f7dbL), tole(0x89044982L), tole(0x88c623b5L),
tole(0x839a6488L), tole(0x82580ebfL), tole(0x801eb0e6L), tole(0x81dcdad1L),
tole(0x8493cc54L), tole(0x8551a663L), tole(0x8717183aL), tole(0x86d5720dL),
tole(0xa9e2d0a0L), tole(0xa820ba97L), tole(0xaa6604ceL), tole(0xaba46ef9L),
tole(0xaeeb787cL), tole(0xaf29124bL), tole(0xad6fac12L), tole(0xacadc625L),
tole(0xa7f18118L), tole(0xa633eb2fL), tole(0xa4755576L), tole(0xa5b73f41L),
tole(0xa0f829c4L), tole(0xa13a43f3L), tole(0xa37cfdaaL), tole(0xa2be979dL),
tole(0xb5c473d0L), tole(0xb40619e7L), tole(0xb640a7beL), tole(0xb782cd89L),
tole(0xb2cddb0cL), tole(0xb30fb13bL), tole(0xb1490f62L), tole(0xb08b6555L),
tole(0xbbd72268L), tole(0xba15485fL), tole(0xb853f606L), tole(0xb9919c31L),
tole(0xbcde8ab4L), tole(0xbd1ce083L), tole(0xbf5a5edaL), tole(0xbe9834edL)},
{
tole(0x00000000L), tole(0xb8bc6765L), tole(0xaa09c88bL), tole(0x12b5afeeL),
tole(0x8f629757L), tole(0x37def032L), tole(0x256b5fdcL), tole(0x9dd738b9L),
tole(0xc5b428efL), tole(0x7d084f8aL), tole(0x6fbde064L), tole(0xd7018701L),
tole(0x4ad6bfb8L), tole(0xf26ad8ddL), tole(0xe0df7733L), tole(0x58631056L),
tole(0x5019579fL), tole(0xe8a530faL), tole(0xfa109f14L), tole(0x42acf871L),
tole(0xdf7bc0c8L), tole(0x67c7a7adL), tole(0x75720843L), tole(0xcdce6f26L),
tole(0x95ad7f70L), tole(0x2d111815L), tole(0x3fa4b7fbL), tole(0x8718d09eL),
tole(0x1acfe827L), tole(0xa2738f42L), tole(0xb0c620acL), tole(0x087a47c9L),
tole(0xa032af3eL), tole(0x188ec85bL), tole(0x0a3b67b5L), tole(0xb28700d0L),
tole(0x2f503869L), tole(0x97ec5f0cL), tole(0x8559f0e2L), tole(0x3de59787L),
tole(0x658687d1L), tole(0xdd3ae0b4L), tole(0xcf8f4f5aL), tole(0x7733283fL),
tole(0xeae41086L), tole(0x525877e3L), tole(0x40edd80dL), tole(0xf851bf68L),
tole(0xf02bf8a1L), tole(0x48979fc4L), tole(0x5a22302aL), tole(0xe29e574fL),
tole(0x7f496ff6L), tole(0xc7f50893L), tole(0xd540a77dL), tole(0x6dfcc018L),
tole(0x359fd04eL), tole(0x8d23b72bL), tole(0x9f9618c5L), tole(0x272a7fa0L),
tole(0xbafd4719L), tole(0x0241207cL), tole(0x10f48f92L), tole(0xa848e8f7L),
tole(0x9b14583dL), tole(0x23a83f58L), tole(0x311d90b6L), tole(0x89a1f7d3L),
tole(0x1476cf6aL), tole(0xaccaa80fL), tole(0xbe7f07e1L), tole(0x06c36084L),
tole(0x5ea070d2L), tole(0xe61c17b7L), tole(0xf4a9b859L), tole(0x4c15df3cL),
tole(0xd1c2e785L), tole(0x697e80e0L), tole(0x7bcb2f0eL), tole(0xc377486bL),
tole(0xcb0d0fa2L), tole(0x73b168c7L), tole(0x6104c729L), tole(0xd9b8a04cL),
tole(0x446f98f5L), tole(0xfcd3ff90L), tole(0xee66507eL), tole(0x56da371bL),
tole(0x0eb9274dL), tole(0xb6054028L), tole(0xa4b0efc6L), tole(0x1c0c88a3L),
tole(0x81dbb01aL), tole(0x3967d77fL), tole(0x2bd27891L), tole(0x936e1ff4L),
tole(0x3b26f703L), tole(0x839a9066L), tole(0x912f3f88L), tole(0x299358edL),
tole(0xb4446054L), tole(0x0cf80731L), tole(0x1e4da8dfL), tole(0xa6f1cfbaL),
tole(0xfe92dfecL), tole(0x462eb889L), tole(0x549b1767L), tole(0xec277002L),
tole(0x71f048bbL), tole(0xc94c2fdeL), tole(0xdbf98030L), tole(0x6345e755L),
tole(0x6b3fa09cL), tole(0xd383c7f9L), tole(0xc1366817L), tole(0x798a0f72L),
tole(0xe45d37cbL), tole(0x5ce150aeL), tole(0x4e54ff40L), tole(0xf6e89825L),
tole(0xae8b8873L), tole(0x1637ef16L), tole(0x048240f8L), tole(0xbc3e279dL),
tole(0x21e91f24L), tole(0x99557841L), tole(0x8be0d7afL), tole(0x335cb0caL),
tole(0xed59b63bL), tole(0x55e5d15eL), tole(0x47507eb0L), tole(0xffec19d5L),
tole(0x623b216cL), tole(0xda874609L), tole(0xc832e9e7L), tole(0x708e8e82L),
tole(0x28ed9ed4L), tole(0x9051f9b1L), tole(0x82e4565fL), tole(0x3a58313aL),
tole(0xa78f0983L), tole(0x1f336ee6L), tole(0x0d86c108L), tole(0xb53aa66dL),
tole(0xbd40e1a4L), tole(0x05fc86c1L), tole(0x1749292fL), tole(0xaff54e4aL),
tole(0x322276f3L), tole(0x8a9e1196L), tole(0x982bbe78L), tole(0x2097d91dL),
tole(0x78f4c94bL), tole(0xc048ae2eL), tole(0xd2fd01c0L), tole(0x6a4166a5L),
tole(0xf7965e1cL), tole(0x4f2a3979L), tole(0x5d9f9697L), tole(0xe523f1f2L),
tole(0x4d6b1905L), tole(0xf5d77e60L), tole(0xe762d18eL), tole(0x5fdeb6ebL),
tole(0xc2098e52L), tole(0x7ab5e937L), tole(0x680046d9L), tole(0xd0bc21bcL),
tole(0x88df31eaL), tole(0x3063568fL), tole(0x22d6f961L), tole(0x9a6a9e04L),
tole(0x07bda6bdL), tole(0xbf01c1d8L), tole(0xadb46e36L), tole(0x15080953L),
tole(0x1d724e9aL), tole(0xa5ce29ffL), tole(0xb77b8611L), tole(0x0fc7e174L),
tole(0x9210d9cdL), tole(0x2aacbea8L), tole(0x38191146L), tole(0x80a57623L),
tole(0xd8c66675L), tole(0x607a0110L), tole(0x72cfaefeL), tole(0xca73c99bL),
tole(0x57a4f122L), tole(0xef189647L), tole(0xfdad39a9L), tole(0x45115eccL),
tole(0x764dee06L), tole(0xcef18963L), tole(0xdc44268dL), tole(0x64f841e8L),
tole(0xf92f7951L), tole(0x41931e34L), tole(0x5326b1daL), tole(0xeb9ad6bfL),
tole(0xb3f9c6e9L), tole(0x0b45a18cL), tole(0x19f00e62L), tole(0xa14c6907L),
tole(0x3c9b51beL), tole(0x842736dbL), tole(0x96929935L), tole(0x2e2efe50L),
tole(0x2654b999L), tole(0x9ee8defcL), tole(0x8c5d7112L), tole(0x34e11677L),
tole(0xa9362eceL), tole(0x118a49abL), tole(0x033fe645L), tole(0xbb838120L),
tole(0xe3e09176L), tole(0x5b5cf613L), tole(0x49e959fdL), tole(0xf1553e98L),
tole(0x6c820621L), tole(0xd43e6144L), tole(0xc68bceaaL), tole(0x7e37a9cfL),
tole(0xd67f4138L), tole(0x6ec3265dL), tole(0x7c7689b3L), tole(0xc4caeed6L),
tole(0x591dd66fL), tole(0xe1a1b10aL), tole(0xf3141ee4L), tole(0x4ba87981L),
tole(0x13cb69d7L), tole(0xab770eb2L), tole(0xb9c2a15cL), tole(0x017ec639L),
tole(0x9ca9fe80L), tole(0x241599e5L), tole(0x36a0360bL), tole(0x8e1c516eL),
tole(0x866616a7L), tole(0x3eda71c2L), tole(0x2c6fde2cL), tole(0x94d3b949L),
tole(0x090481f0L), tole(0xb1b8e695L), tole(0xa30d497bL), tole(0x1bb12e1eL),
tole(0x43d23e48L), tole(0xfb6e592dL), tole(0xe9dbf6c3L), tole(0x516791a6L),
tole(0xccb0a91fL), tole(0x740cce7aL), tole(0x66b96194L), tole(0xde0506f1L)},
};

/**
 * Calculate the PNG 32 bit CRC on a buffer
 */
uint32_t bcrc32(unsigned char*buf, int len)
{
# ifdef HAVE_LITTLE_ENDIAN
#  define DO_CRC(x) crc = tab[0][(crc ^ (x)) & 255 ] ^ (crc >> 8)
#  define DO_CRC4 crc = tab[3][(crc) & 255 ] ^ \
                tab[2][(crc >> 8) & 255 ] ^ \
                tab[1][(crc >> 16) & 255 ] ^ \
                tab[0][(crc >> 24) & 255 ]
# else
#  define DO_CRC(x) crc = tab[0][((crc >> 24) ^ (x)) & 255] ^ (crc << 8)
#  define DO_CRC4 crc = tab[0][(crc) & 255 ] ^ \
                tab[1][(crc >> 8) & 255 ] ^ \
                tab[2][(crc >> 16) & 255 ] ^ \
                tab[3][(crc >> 24) & 255 ]
# endif
        const uint32_t *b;
        size_t    rem_len;
        uint32_t crc = tole(~0);

        /* Align it */
        if ((intptr_t)buf & 3 && len) {
                do {
                        DO_CRC(*buf++);
                } while ((--len) && ((intptr_t)buf)&3);
        }
        rem_len = len & 3;
        /* load data 32 bits wide, xor data 32 bits wide. */
        b = (const uint32_t *)buf;
        len = len >> 2;
        for (--b; len; --len) {
                crc ^= *++b; /* use pre increment for speed */
                DO_CRC4;
        }
        len = rem_len;
        /* And the last few bytes */
        if (len) {
                uint8_t *p = (uint8_t *)(b + 1) - 1;
                do {
                        DO_CRC(*++p); /* use pre increment for speed */
                } while (--len);
        }
        return tole(crc) ^ ~0;
}


#ifdef CRC32_SUM

static void usage()
{
   fprintf(stderr,
"\n"
"Usage: crc32 <data-file>\n"
"       -?          print this message.\n"
"\n\n");

   exit(1);
}

/**
 * Reads a single ASCII file and prints the HEX md5 sum.
 */
#include <stdio.h>
int main(int argc, char *argv[])
{
   FILE *fd;
   char buf[5000];
   int ch;

   while ((ch = getopt(argc, argv, "h?")) != -1) {
      switch (ch) {
      case 'h':
      case '?':
      default:
         usage();
      }
   }

   argc -= optind;
   argv += optind;

   if (argc < 1) {
      printf("Must have filename\n");
      exit(1);
   }

   fd = fopen(argv[0], "rb");
   if (!fd) {
      printf("Could not open %s: ERR=%s\n", argv[0], strerror(errno));
      exit(1);
   }
   uint32_t res;
   while (fgets(buf, sizeof(buf), fd)) {
      res = bcrc32((unsigned char *)buf, strlen(buf));
      printf("%02x\n", res);
   }
   printf("  %s\n", argv[0]);
   fclose(fd);
}
#endif
