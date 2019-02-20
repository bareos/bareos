/*
 * Copyright (c) 1998,1999,2000
 *      Traakan, Inc., Los Altos, CA
 *      All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Project:  NDMJOB
 * Ident:    $Id: $
 *
 * Description:
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 17.1 Medium-changer device model
 *
 * Medium changer devices mechanize the movement of media to and from
 * primary devices (such as disk or tape drives) and other locations
 * within the range of the medium changer. The medium changer command set
 * is based on a physical model of this functionality.
 *
 * This command set supports varied physical implementations of the medium
 * changer function. Most of these variations are hidden from the
 * initiator by the high level of function provided by the MOVE MEDIUM and
 * EXCHANGE MEDIUM commands and by the generalized nature of the element
 * addressing scheme. However, initiators may need to be aware of the
 * capabilities of the particular medium changer device. These
 * characteristics and capabilities are conveyed via MODE SENSE pages.
 *
 * 17.1.1 Medium-changer elements
 *
 * The medium-changer command set uses as its address space the set of
 * physical locations and mechanisms within the scope of a medium changer
 * device. The term element is used throughout this clause to refer to one
 * member of the medium changer address space. Each element is a discrete
 * physical entity that may hold zero or one physical unit of media - one
 * disk cartridge, one spool of tape, etc. Element addresses do not extend
 * across multiple physical units of media. Likewise, element addresses
 * are independent of any logical partitioning that the primary device may
 * support within a physical unit of media.
 *
 * A medium changer is viewed as a set of addressable elements, each of
 * which may contain a unit of media or be used to move a unit of media.
 * Each medium changer element has a unique 16 bit element address. Each
 * element is an instance of one of four classes or element types.
 *
 *    a)     MEDIUM TRANSPORT ELEMENT
 *    b)     STORAGE ELEMENT
 *    c)     IMPORT EXPORT ELEMENT
 *    d)     DATA TRANSFER ELEMENT
 *
 * Units of media (cartridges, cassettes, caddies, etc.) are referred to
 * only indirectly by this device model.  Units of media can be moved to
 * or from any of the elements of the medium changer device.  The presence
 * of media at the various elements in a medium changer can be sensed.  In
 * order to ensure exclusive access to a unit of media, the element where
 * the unit of media is located (the element address) must be reserved.
 *
 * Elements of the medium transport, import export and (rarely) data
 * transport types may not provide independent storage for medium.  The
 * capabilities of a particular medium changer in this respect can be
 * sensed via the device capabilities page of the mode sense data. The
 * following hypothetical medium changer implementation illustrates one
 * case of an element not providing independent storage for medium.
 * Consider a medium changer which has a carousel style storage for
 * medium. The import export function could be provided by a port which
 * allows operator access to one of the storage elements.  In such a
 * device, the MOVE ELEMENT command from storage element to import export
 * element would rotate the carousel to align the addressed storage
 * element to the import export position. In this case the import export
 * element does not provide independent storage but rather access to one
 * of the storage elements.
 *
 * 17.1.1.1 Medium transport elements
 *
 * Medium transport elements address the functions of the medium changer
 * device that perform the movement of units of media. Where a medium
 * transport element can serve (even temporarily) as a storage location
 * for medium, the location of each unit of media must have a separate
 * element address.
 *
 * In larger medium changer devices, the medium movement functions may be
 * performed by multiple independent robotics subsystems. Each of these
 * may have a number of medium transport element addresses. The element
 * addresses within each subsystem shall be contiguous. Any of the element
 * addresses within a subsystem may be used interchangeably in the medium
 * transport element address field of MOVE MEDIUM and EXCHANGE MEDIUM
 * commands. An initiator may determine the capabilities of the medium
 * movement facilities of a medium changer device via the transport
 * geometry MODE SENSE page, see 17.3.3.3.
 *
 * Element address zero is reserved for use in the medium transport
 * element address field of MOVE MEDIUM and EXCHANGE MEDIUM commands to
 * direct the medium changer to use a default or medium changer selected
 * medium transport element.
 *
 * In some implementations, medium transport elements may be source and/or
 * destination addresses in MOVE MEDIUM and EXCHANGE MEDIUM commands.
 * They may or may not provide independent storage of a unit of media.
 * See the device capabilities MODE SENSE page, see 17.3.3.
 *
 * 17.1.1.2 Storage elements
 *
 * Storage elements are locations of units of media while not in some
 * other element type.  Medium in storage elements is available for access
 * by medium transport elements.
 *
 * Storage elements may be source and/or destination addresses in MOVE
 * MEDIUM and EXCHANGE MEDIUM commands.
 *
 * 17.1.1.3 Import export elements
 *
 * Import export elements are locations of units of media which are being
 * inserted into or withdrawn from the medium changer device.  Medium in
 * these elements is accessible by both medium transport elements, by the
 * operator, or by another independent medium changer device.
 *
 * Import export elements may be source and/or destination addresses in
 * MOVE MEDIUM and EXCHANGE MEDIUM commands. They may or may not provide
 * independent storage of a unit of media, see the device capabilities
 * MODE SENSE page, see 17.3.3.
 *
 * Particular import export elements may be capable of either import
 * actions, export actions, both or neither (if an element is not
 * present).
 *
 * 17.1.1.4 Data transfer element
 *
 * Data transfer elements are locations of the primary devices which are
 * capable of reading or writing the medium. Data transfer elements may
 * also be viewed as medium changer element addresses of units of media
 * loaded in or available for loading in or removal from primary devices
 * such as disk or tape drives. Note that the medium changer function
 * specified in this clause does not control the primary device. That is
 * the responsibility of the system.
 *
 * Data transfer elements may be source and/or destination addresses in
 * MOVE MEDIUM and EXCHANGE MEDIUM commands. They may or may not provide
 * independent storage of a unit of media, see the device capabilities
 * MODE SENSE page, see 17.3.3.
 */

/*
 *                        Table 333 - Element type code
 *     +=============-===================================================+
 *     |    Code     |  Description                                      |
 *     |-------------+---------------------------------------------------|
 *     |      0h     |  All element types reported, (valid in CDB only)  |
 *     |      1h     |  Medium transport element                         |
 *     |      2h     |  Storage element                                  |
 *     |      3h     |  Import export element                            |
 *     |      4h     |  Data transfer element                            |
 *     |   5h - Fh   |  Reserved                                         |
 *     +=================================================================+
 */

#define SMC_ELEM_TYPE_ALL 0
#define SMC_ELEM_TYPE_MTE 1
#define SMC_ELEM_TYPE_SE 2
#define SMC_ELEM_TYPE_IEE 3
#define SMC_ELEM_TYPE_DTE 4


/*
 * 17.1.5 Volume tags
 *
 * The read element status descriptor format for all element types
 * includes two sets of fields that contain volume tag information. These
 * optional fields are used to report media identification information
 * that the medium changer has acquired either by reading an external
 * label (e.g. bar code labels), by a SEND VOLUME TAG command or by other
 * means which may be vendor unique. The same volume tag information shall
 * be available to all initiators whether assigned by that initiator, by
 * some other initiator or by the media changer itself.
 *
 * Volume tag information provides a means to confirm the identity of a
 * unit of media that resides in a medium changer element. This command
 * set does not define any direct addressing of units of media based on
 * these fields. However, commands are defined that provide translation
 * between volume tag information and the element address where that unit
 * of media currently resides.
 *
 * The medium changer command set definition does not impose the
 * requirement that volume tag information be unique over the units of
 * media within the scope of the changer device.
 *
 * The following commands support the optional volume tag functionality:
 *    a)     SEND VOLUME TAG - either as a translation request or to associate
 *           a volume tag with the unit of media currently residing at an
 *           element address.
 *    b)     REQUEST VOLUME ELEMENT ADDRESS - return the element address
 *           currently associated with the volume tag information transferred
 *           with the last send volume tag command.
 *    c)     READ ELEMENT STATUS - optionally reports volume tag information
 *           for all element types.
 *    d)     MOVE MEDIUM and EXCHANGE MEDIUM commands - if volume tags are
 *           implemented, these commands must retain the association between
 *           volume tag information and units of media as they are moved from
 *           element to element.
 *
 * 17.1.5.1 Volume tag format
 *
 * Volume tag information consists of a volume identifier field of 32
 * bytes plus a volume sequence number field of 2 bytes. The volume
 * identifier shall consist of a left justified sequence of ASCII
 * characters. Unused positions shall be blank (20h) filled.  In order for
 * the SEND VOLUME TAG translate with template to work the characters '*'
 * and'?' (2Ah and 3Fh) must not appear in volume identification data and
 * there must be no blanks (20h) within the significant part of the volume
 * identifier. If volume tag information for a particular element is
 * undefined, the volume identifier field shall be zero.
 *
 * The volume sequence number is a 2 byte integer field. If the volume
 * sequence number is not used this field shall be zero.  The volume tag
 * contents are independent of the volume identification information
 * recorded on the media.
 *
 *  NOTE 199 For compatibility with the volume identifier defined by volume
 *  and file structure standards, it is recommended that the characters in the
 *  significant non-blank portion of the volume identifier field be restricted
 *  to the set: '0'..'9', 'A'..'Z', and '_' (30h .. 39h, 41h .. 5Ah, 5Fh).
 *  Specific systems may have differing requirements that may take precedence
 *  over this recommendation.
 *
 * Table 326 defines the fields within the 36 byte primary and alternate
 * volume tag information fields found in READ ELEMENT STATUS descriptors
 * and in the data format for the SEND VOLUME TAG command.
 *
 *                   Table 326 - Volume tag information format
 * +=====-=======-=======-=======-========-========-========-=======-========+
 * |  Bit|   7   |   6   |   5   |   4    |   3    |   2    |   1   |   0    |
 * |Byte |       |       |       |        |        |        |       |        |
 * |=====+===================================================================|
 * | 0   |                                                                   |
 * |- - -+---                Volume identification field                  ---|
 * | 31  |                                                                   |
 * |-----+-------------------------------------------------------------------|
 * | 32  |                                                                   |
 * |- - -+---                         Reserved                            ---|
 * | 33  |                                                                   |
 * |-----+-------------------------------------------------------------------|
 * | 34  | (MSB)                                                             |
 * |-----+---                  Volume sequence number                     ---|
 * | 35  |                                                             (LSB) |
 * +=========================================================================+
 *
 *
 * 17.1.5.2 Primary and alternate volume tag information
 *
 * Element status descriptors as reported by the READ ELEMENT STATUS
 * command define a primary volume tag and an alternate volume tag.
 * Alternate volume tag information provides a means for a system to use
 * different volume identification information for each side of double
 * sided media. In such a system, the primary volume tag information
 * refers to the logical medium accessible via a MOVE MEDIUM command
 * without the invert bit set. The alternate volume tag information refers
 * to the other side of the media, i.e. the side that would be accessed
 * via a MOVE MEDIUM command with the invert bit set.
 */

#define SMC_VOL_TAG_LEN 36

struct smc_raw_volume_tag {
  unsigned char volume_id[32];
  unsigned char resv32[2];
  unsigned char volume_seq[2];
};


/*
 * 17.1.4 Element status maintenance requirements
 *
 * If the medium changer device chooses to implement the READ ELEMENT
 * STATUS command, the medium changer device must be capable of reporting
 * the various data (i.e. full, error, etc.) required by each page type.
 * The medium changer may maintain this information at all times or
 * regenerate it in response to the READ ELEMENT STATUS command. The
 * INITIALIZE ELEMENT STATUS command can be used to force regeneration of
 * this information.
 */

/*
 * 17.2.5 READ ELEMENT STATUS command
 *
 * The READ ELEMENT STATUS command (see table 332) requests that the
 * target report the status of its internal elements to the initiator.
 *
 *                    Table 332 - READ ELEMENT STATUS command
 * +====-=======-========-========-========-========-========-========-=======+
 * | Bit|  7    |   6    |   5    |   4    |   3    |   2    |   1    |   0   |
 * |Byte|       |        |        |        |        |        |        |       |
 * |====+=====================================================================|
 * | 0  |                          Operation code (B8h)                       |
 * |----+---------------------------------------------------------------------|
 * | 1  |Logical unit number      | VolTag |        Element type code         |
 * |----+---------------------------------------------------------------------|
 * | 2  |(MSB)                                                                |
 * |----+--                        Starting element address                 --|
 * | 3  |                                                                (LSB)|
 * |----+---------------------------------------------------------------------|
 * | 4  |(MSB)                                                                |
 * |----+--                        Number of elements                       --|
 * | 5  |                                                                (LSB)|
 * |----+---------------------------------------------------------------------|
 * | 6  |                          Reserved                                   |
 * |----+---------------------------------------------------------------------|
 * | 7  |(MSB)                                                                |
 * |----+--                                                                 --|
 * | 8  |                          Allocation length                          |
 * |----+--                                                                 --|
 * | 9  |                                                                (LSB)|
 * |----+---------------------------------------------------------------------|
 * |10  |                          Reserved                                   |
 * |----+---------------------------------------------------------------------|
 * |11  |                          Control                                    |
 * +==========================================================================+
 *
 *
 * A volume tag (VolTag) bit of one indicates that the target shall report
 * volume tag information if this feature is supported. A value of zero
 * indicates that volume tag information shall not be reported. If the
 * volume tag feature is not supported this field shall be treated as
 * reserved.
 *
 * The element type code field specifies the particular element type(s)
 * selected for reporting by this command.  A value of zero specifies that
 * status for all element types shall be reported.  The element type codes
 * are defined in table 333.
 *
 *                         Table 333 - Element type code
 *      (SEE ABOVE)
 *
 * The starting element address specifies the minimum element address to
 * report. Only elements with an element type code permitted by the
 * element type code specification, and an element address greater than or
 * equal to the starting element address shall be reported. Element
 * descriptor blocks are not generated for undefined element addresses.
 *
 * The number of elements specifies the maximum number of element
 * descriptors to be created by the target for this command. The value
 * specified by this field is not the range of element addresses to be
 * considered for reporting but rather the number of defined elements to
 * report. If the allocation length is not sufficient to transfer all the
 * element descriptors, the target shall transfer all those descriptors
 * that can be completely transferred and this shall not be considered an
 * error.
 *
 * 17.2.5.1 Element status data
 *
 * The data returned by the READ ELEMENT STATUS command is defined in
 * table 334 and 17.2.5.3 through 17.2.5.6.  Element status data consists
 * of an eight-byte header, (see table 334) followed by one or more
 * element status pages.
 *
 *                        Table 334 - Element status data
 * +====-=======-========-========-========-========-========-========-=======+
 * | Bit|  7    |   6    |   5    |   4    |   3    |   2    |   1    |   0   |
 * |Byte|       |        |        |        |        |        |        |       |
 * |====+=====================================================================|
 * | 0  |(MSB)                                                                |
 * |----+--                  First element address reported                 --|
 * | 1  |                                                                (LSB)|
 * |----+---------------------------------------------------------------------|
 * | 2  |(MSB)                                                                |
 * |----+--                    Number of elements available                 --|
 * | 3  |                                                                (LSB)|
 * |----+---------------------------------------------------------------------|
 * | 4  |                             Reserved                                |
 * |----+---------------------------------------------------------------------|
 * | 5  |(MSB)                                                                |
 * |----+--                                                                 --|
 * | 6  |                    Byte count of report available                   |
 * |----+--                        (all pages, n - 7 )                      --|
 * | 7  |                                                                (LSB)|
 * |====+=====================================================================|
 * | 8  |                                                                     |
 * |- - +--                     Element status page(s)                      --|
 * | n  |                                                                     |
 * +==========================================================================+
 *
 *
 * The first element address reported field indicates the element address
 * of the element with the smallest element address found to meet the CDB
 * request.
 *
 * The number of elements available field indicates the number of elements
 * meeting the request in the command descriptor block.  The status for
 * these elements is returned if sufficient allocation length was
 * specified.
 *
 * The byte count of report available field indicates the number of bytes
 * of element status page data available for all elements meeting the
 * request in the command descriptor block.  This value shall not be
 * adjusted to match the allocation length available.
 *
 *    NOTE 202 - The READ ELEMENT STATUS command can be issued with an
 *    allocation length of eight bytes in order to determine the allocation
 *    length required to transfer all the element status data specified by the
 *    command.
 *
 * Figure 28 provides an illustration of the element status data
 * structure.
 *
 *           Figure 28 - Illustration of element status data structure
 *
 *
 * 17.2.5.2 Element status page
 *
 * The element status page is defined in table 335.  Each element status
 * page includes an eight-byte header followed by one or more element
 * descriptor blocks.  The header includes the element type code, the
 * length of each descriptor block and the number of bytes of element
 * descriptor information that follow the header for this element type.
 *
 *                        Table 335 - Element status page
 * +====-=======-========-========-========-========-========-========-=======+
 * | Bit|  7    |   6    |   5    |   4    |   3    |   2    |   1    |   0   |
 * |Byte|       |        |        |        |        |        |        |       |
 * |====+=====================================================================|
 * | 0  |                          Element type code                          |
 * |----+---------------------------------------------------------------------|
 * | 1  |PVolTag| AVolTag|                      Reserved                      |
 * |----+---------------------------------------------------------------------|
 * | 2  |(MSB)                                                                |
 * |----+--                    Element descriptor length                    --|
 * | 3  |                                                                (LSB)|
 * |----+---------------------------------------------------------------------|
 * | 4  |                             Reserved                                |
 * |----+---------------------------------------------------------------------|
 * | 5  |(MSB)                                                                |
 * |----+--                                                                 --|
 * | 6  |               Byte count of descriptor data available               |
 * |----+--                      (this page, n - 7)                         --|
 * | 7  |                                                                (LSB)|
 * |====+=====================================================================|
 * | 8  |                                                                     |
 * |- - +--                     Element descriptor(s)                       --|
 * | n  |                                                                     |
 * +==========================================================================+
 *
 *
 * The element type code field indicates the element type reported by this
 * page.
 *
 * A primary volume tag (PVolTag) bit of one indicates that the primary
 * volume tag information field is present in each of the following
 * element descriptor blocks. A value of zero indicates that these bytes
 * are omitted from the element descriptors that follow.
 *
 * An alternate volume tag (AVolTag) bit of one indicates that the
 * alternate volume tag information field is present in each of the
 * following element descriptor blocks. A value of zero indicates that
 * these bytes are omitted from the element descriptors that follow.
 *
 * The element descriptor length field indicates the number of bytes in
 * each element descriptor.
 *
 * The byte count of descriptor data available field indicates the number
 * of bytes of element descriptor data available for elements of this
 * element type meeting the request in the CDB. This value shall not be
 * adjusted to match the allocation length available.
 *
 * Each element descriptor includes the element address and status flags;
 * it may also contain sense code information as well as other information
 * depending on the element type (see 17.2.5.3 through 17.2.5.6).
 *
 * 17.2.5.3 Medium transport element descriptor
 *
 * Table 336 defines the medium transport element descriptor.
 *
 *                Table 336 - Medium transport element descriptor
 * +====-======-========-========-========-========-========-========-=======+
 * | Bit|  7   |   6    |   5    |   4    |   3    |   2    |   1    |   0   |
 * |Byte|      |        |        |        |        |        |        |       |
 * |====+====================================================================|
 * | 0  |(MSB)                                                               |
 * |----+--                     Element address                            --|
 * | 1  |                                                               (LSB)|
 * |----+--------------------------------------------------------------------|
 * | 2  |          Reserved                        | Except |Reserved|  Full |
 * |----+--------------------------------------------------------------------|
 * | 3  |                          Reserved                                  |
 * |----+--------------------------------------------------------------------|
 * | 4  |                    Additional sense code                           |
 * |----+--------------------------------------------------------------------|
 * | 5  |                Additional sense code qualifier                     |
 * |----+--------------------------------------------------------------------|
 * | 6  |                                                                    |
 * | - -+--                        Reserved                                --|
 * | 8  |                                                                    |
 * |----+--------------------------------------------------------------------|
 * | 9  |SValid| Invert |                      Reserved                      |
 * |----+--------------------------------------------------------------------|
 * |10  |(MSB)                                                               |
 * |----+--               Source storage element address                   --|
 * |11  |                                                              (LSB) |
 * |----+--------------------------------------------------------------------|
 * |12  |                                                                    |
 * | - -+--               Primary volume tag information                   --|
 * |47  |                 (Field omitted if PVolTag = 0)                     |
 * |----+--------------------------------------------------------------------|
 * |48  |                                                                    |
 * | - -+--              Alternate volume tag information                  --|
 * |83  |                 (Field omitted if AVolTag = 0)                     |
 * |----+--------------------------------------------------------------------|
 * |84  |                                                                    |
 * | - -+--                         Reserved                               --|
 * |87  |  (Field moved up if volume tag information field(s) are omitted.)  |
 * |----+--------------------------------------------------------------------|
 * |88  |                                                                    |
 * | - -+--                       Vendor-specific                          --|
 * |z-1 |  (Field moved up if volume tag information field(s) are omitted.)  |
 * +=========================================================================+
 *
 *
 * The element address field gives the address of the medium changer
 * element whose status is reported by this element descriptor block.
 *
 * An exception (Except) bit of one indicates the element is in an
 * abnormal state.  An exception bit of zero indicates the element is in a
 * normal state.  If this bit is one, information on the abnormal state
 * may be available in the additional sense code and additional sense code
 * qualifier bytes.
 *
 * A full bit value of one indicates that the element contains a unit of
 * media.  A value of zero indicates that the element does not contain a
 * unit of media.
 *
 * The additional sense code field may provide specific information on an
 * abnormal element state.  The values in this field are as defined for
 * the additional sense code of the REQUEST SENSE data (see 8.2.14.3).
 *
 * The additional sense code qualifier field may provide more detailed
 * information on an abnormal element state.  The values in this field are
 * as defined for the additional sense code qualifier of the REQUEST SENSE
 * data (see 8.2.14.4).
 *
 * A source valid (SValid) bit value of one indicates that the source
 * storage element address field and the invert bit information are valid.
 * A value of zero indicates that the values in these fields are not
 * valid.
 *
 * An invert bit value of one indicates that the unit of media now in this
 * element was inverted by MOVE MEDIUM or EXCHANGE MEDIUM operations since
 * it was last in the source storage element.  A value of zero indicates
 * that no inversion occurred during the operation.
 *
 * The source storage element address field provides the address of the
 * last storage element this unit of media was moved.  This field is valid
 * only if the SValid bit is one.
 *
 * The primary and alternate volume tag information fields provide for
 * identifying the unit of media residing in this element (see 17.1.5.).
 * Either or both of these fields may be omitted for all the element
 * descriptor blocks that comprise an element status page as indicated by
 * the PVolTag and AVolTag bits in the element status page header.
 *
 * 17.2.5.4 Storage element descriptor
 *
 * Table 337 defines the storage element descriptor.
 *
 *                     Table 337 - Storage element descriptor
 * +=====-======-========-========-========-========-========-========-=======+
 * | Bit|  7   |   6    |   5    |   4    |   3    |   2    |   1    |   0   |
 * |Byte|      |        |        |        |        |        |        |       |
 * |====+====================================================================|
 * | 0  |(MSB)                                                               |
 * |----+--                     Element address                            --|
 * | 1  |                                                               (LSB)|
 * |----+--------------------------------------------------------------------|
 * | 2  |          Reserved               | Access | Except |Reserved|  Full |
 * |----+--------------------------------------------------------------------|
 * | 3  |                          Reserved                                  |
 * |----+--------------------------------------------------------------------|
 * | 4  |                     Additional sense code                          |
 * |----+--------------------------------------------------------------------|
 * | 5  |                 Additional sense code qualifier                    |
 * |----+--------------------------------------------------------------------|
 * | 6  |                                                                    |
 * | - -+--                        Reserved                                --|
 * | 8  |                                                                    |
 * |----+--------------------------------------------------------------------|
 * | 9  |SValid| Invert |                      Reserved                      |
 * |----+--------------------------------------------------------------------|
 * |10  |(MSB)                                                               |
 * |----+--                   Source element address                       --|
 * |11  |                                                              (LSB) |
 * |----+--------------------------------------------------------------------|
 * |12  |                                                                    |
 * | - -+--                Primary volume tag information                  --|
 * |47  |                  (Field omitted if PVolTag = 0)                    |
 * |----+--------------------------------------------------------------------|
 * |48  |                                                                    |
 * | - -+--               Alternate volume tag information                 --|
 * |83  |                  (Field omitted if PVolTag = 0)                    |
 * |----+--------------------------------------------------------------------|
 * |84  |                                                                    |
 * | - -+--                        Reserved                                --|
 * |87  |  (Field moved up if volume tag information field(s) are omitted.)  |
 * |----+--------------------------------------------------------------------|
 * |88  |                                                                    |
 * | - -+--                     Vendor unique                              --|
 * |z-1 |  (Field moved up if volume tag information field(s) are omitted.)  |
 * +=========================================================================+
 *
 *
 * An access bit value of one indicates that access to the element by a
 * medium transport element is allowed.  An access bit of zero indicates
 * that access to the element by the medium transport element is denied.
 *
 * The source storage element address field provides the address of the
 * last storage element this unit of media was moved from. This element
 * address value may or may not be the same as this element. This field is
 * valid only if the SValid bit is one.
 *
 * For fields not defined in this subclause, see 17.2.5.3.
 *
 * 17.2.5.5 Import export element descriptor
 *
 * Table 338 defines the import export element descriptor.
 *
 *                  Table 338 - Import export element descriptor
 * +====-======-========-========-========-========-========-========-=======+
 * | Bit|  7   |   6    |   5    |   4    |   3    |   2    |   1    |   0   |
 * |Byte|      |        |        |        |        |        |        |       |
 * |====+====================================================================|
 * | 0  |(MSB)                                                               |
 * |----+--                       Element address                          --|
 * | 1  |                                                               (LSB)|
 * |----+--------------------------------------------------------------------|
 * | 2  |   Reserved    | InEnab | ExEnab | Access | Except | ImpExp |  Full |
 * |----+--------------------------------------------------------------------|
 * | 3  |                           Reserved                                 |
 * |----+--------------------------------------------------------------------|
 * | 4  |                      Additional sense code                         |
 * |----+--------------------------------------------------------------------|
 * | 5  |                  Additional sense code qualifier                   |
 * |----+--------------------------------------------------------------------|
 * | 6  |                                                                    |
 * |----+--                         Reserved                               --|
 * | 8  |                                                                    |
 * |----+--------------------------------------------------------------------|
 * | 9  |SValid| Invert |                      Reserved                      |
 * |----+--------------------------------------------------------------------|
 * |10  |(MSB)                                                               |
 * |----+--                 Source storage element address                 --|
 * |11  |                                                              (LSB) |
 * |----+--------------------------------------------------------------------|
 * |12  |                                                                    |
 * | - -+--                 Primary volume tag information                 --|
 * |47  |                   (Field omitted if PVolTag = 0)                   |
 * |----+--------------------------------------------------------------------|
 * |48  |                                                                    |
 * | - -+--                Alternate volume tag information                --|
 * |83  |                   (Field omitted if PVolTag = 0)                   |
 * |----+--------------------------------------------------------------------|
 * |84  |                                                                    |
 * | - -+--                         Reserved                               --|
 * |87  |   (Field moved up if volume tag information field(s) are omitted.) |
 * |----+--------------------------------------------------------------------|
 * |88  |                                                                    |
 * | - -+--                        Vendor unique                           --|
 * |z-1 |  (Field moved up if volume tag information field(s) are omitted.)  |
 * +=========================================================================+
 *
 *
 * An import enable (InEnab) bit of one indicates that the import export
 * element supports movement of media into the scope of the medium changer
 * device.  An InEnab bit of zero indicates that this element does not
 * support import actions.
 *
 * An export enable (ExEnab) bit of one indicates that the import export
 * element supports movement of media out of the scope of the medium
 * changer device. An ExEnab bit of zero indicates that this element does
 * not support export actions.
 *
 * An access bit of one indicates that access to the import export element
 * by a medium transport element is allowed.  An access bit of zero
 * indicates access to the import export element by medium transport
 * elements is denied.
 *
 *    NOTE 203 An example of when access would be denied is when the operator
 *    has exclusive access to the import export element.
 *
 * An import export (ImpExp) bit of one indicates the unit of media in the
 * import export element was placed there by an operator.  An ImpExp bit
 * of zero indicates the unit of media in the import export element was
 * placed there by the medium transport element.
 *
 * For fields not defined in this clause, see 17.2.5.3.
 *
 * 17.2.5.6 Data transfer element descriptor
 *
 * Table 339 defines the data transfer element descriptor.
 *
 *                  Table 339 - Data transfer element descriptor
 * +====-=======-========-========-========-========-========-========-=======+
 * | Bit|  7    |   6    |   5    |   4    |   3    |   2    |   1    |   0   |
 * |Byte|       |        |        |        |        |        |        |       |
 * |====+=====================================================================|
 * | 0  |(MSB)                                                                |
 * |----+--                      Element address                            --|
 * | 1  |                                                                (LSB)|
 * |----+---------------------------------------------------------------------|
 * | 2  |           Reserved               | Access | Except |Reserved|  Full |
 * |----+---------------------------------------------------------------------|
 * | 3  |                           Reserved                                  |
 * |----+---------------------------------------------------------------------|
 * | 4  |                      Additional sense code                          |
 * |----+---------------------------------------------------------------------|
 * | 5  |                  Additional sense code qualifier                    |
 * |----+---------------------------------------------------------------------|
 * | 6  Not bus|Reserved|ID valid|LU valid|Reserved|    Logical unit number   |
 * |----+---------------------------------------------------------------------|
 * | 7  |                        SCSI bus address                             |
 * |----+---------------------------------------------------------------------|
 * | 8  |                            Reserved                                 |
 * |----+---------------------------------------------------------------------|
 * | 9  |SValid | Invert |                      Reserved                      |
 * |----+---------------------------------------------------------------------|
 * |10  |(MSB)                                                                |
 * |----+--              Source storage element address                     --|
 * |11  |                                                               (LSB) |
 * |----+---------------------------------------------------------------------|
 * |12  |                                                                     |
 * | - -+--                 Primary volume tag information                  --|
 * |47  |                   (Field omitted if PVolTag = 0)                    |
 * |----+---------------------------------------------------------------------|
 * |48  |                                                                     |
 * | - -+--                Alternate volume tag information                 --|
 * |83  |                   (Field omitted if PVolTag = 0)                    |
 * |----+---------------------------------------------------------------------|
 * |84  |                                                                     |
 * | - -+--                         Reserved                                --|
 * |87  |   (Field moved up if volume tag information field(s) are omitted.)  |
 * |----+---------------------------------------------------------------------|
 * |88  |                                                                     |
 * | - -+--                        Vendor unique                            --|
 * |z-1 |   (Field moved up if volume tag information field(s) are omitted.)  |
 * +==========================================================================+
 *
 *
 * An access bit value of one indicates access to the data transfer
 * element by the medium transport element is allowed.  A value of zero
 * indicates access to the data transfer element by a medium transport
 * element is denied.
 *
 *    NOTE 204 Access to the data transfer element by medium transport elements
 *    might be denied if a data transfer operation was under way. Note that a
 *    one value in this bit may not be sufficient to ensure a successful
 *   operation. This bit can only reflect the best information available to the
 *    medium changer device, which may not accurately reflect the state of the
 *    primary (data transfer) device.
 *
 * A not this bus (not bus) bit value of one indicates that the SCSI bus
 * address and logical unit number values are not valid for the SCSI bus
 * used to select the medium changer device. A not bus bit value of zero
 * indicates that the SCSI address and logical unit values, if valid, are
 * on the same bus as the medium changer device.
 *
 * An ID Valid bit value of one indicates that the SCSI bus address field
 * contains valid information. An LU Valid bit value of one indicates that
 * the logical unit number field contains valid information.
 *
 * The SCSI bus address field, if valid, provides the SCSI address (binary
 * representation) of the primary device served by the medium changer at
 * this element address.
 *
 * The logical unit number field, if valid, provides the logical unit
 * number within the SCSI bus device of the primary device served by the
 * medium changer at this element address.
 *
 * For fields not defined in this clause, see 17.2.5.3.
 */

struct smc_raw_element_status_data_header {
  unsigned char first_elem[2];
  unsigned char n_elem[2];
  unsigned char resv4;
  unsigned char byte_count[3];
};

struct smc_raw_element_status_page_header {
  unsigned char element_type;
  unsigned char flag1;
#define SMC_RAW_ESP_F1_PVolTag 0x80
#define SMC_RAW_ESP_F1_AVolTag 0x40
  unsigned char elem_desc_len[2];
  unsigned char resv4;
  unsigned char byte_count[3];
};

struct smc_raw_element_descriptor {
  unsigned char element_address[2];
  unsigned char flags2;
#define SMC_RAW_ED_F2_Full 0x01
#define SMC_RAW_ED_F2_ImpExp 0x02
#define SMC_RAW_ED_F2_Except 0x04
#define SMC_RAW_ED_F2_Access 0x08
#define SMC_RAW_ED_F2_ExEnab 0x10
#define SMC_RAW_ED_F2_InEnab 0x20

  unsigned char resv3;
  unsigned char asc;
  unsigned char ascq;
  unsigned char flags6;
#define SMC_RAW_ED_F6_LUN 0x07
#define SMC_RAW_ED_F6_LU_valid 0x10
#define SMC_RAW_ED_F6_ID_valid 0x20
#define SMC_RAW_ED_F6_Not_bus 0x80

  unsigned char scsi_sid;

  unsigned char resv8;

  unsigned char flags9;
#define SMC_RAW_ED_F9_Invert 0x40
#define SMC_RAW_ED_F9_SValid 0x80

  unsigned char src_se_addr[2];

  /*
   * primary_vol_tag (optional)
   * alternate_vol_tag (optional)
   * resv84
   * vendor_specific
   */
  unsigned char data[SMC_VOL_TAG_LEN + SMC_VOL_TAG_LEN + 4 + 4];
};


/*
 * 17.3.3.2 Element address assignment page
 *
 * The element address assignment page (see table 352) is used to assign
 * addresses to the elements of the medium changer (MODE SELECT) and to
 * report those assignments (MODE SENSE). This page also defines the
 * number of each type of element present.
 *
 *                  Table 352 - Element address assignment page
 * +====-=======-========-========-========-========-========-========-=======+
 * | Bit|  7    |   6    |   5    |   4    |   3    |   2    |   1    |   0   |
 * |Byte|       |        |        |        |        |        |        |       |
 * |====+=======+========+====================================================|
 * | 0  |  PS   |Reserved|         Page code (1Dh)                            |
 * |----+---------------------------------------------------------------------|
 * | 1  |                          Parameter length (12h)                     |
 * |----+---------------------------------------------------------------------|
 * | 2  |(MSB)                                                                |
 * |----+--                        Medium transport element address         --|
 * | 3  |                                                                (LSB)|
 * |----+---------------------------------------------------------------------|
 * | 4  |(MSB)                                                                |
 * |----+--                        Number of medium transport elements      --|
 * | 5  |                                                                (LSB)|
 * |----+---------------------------------------------------------------------|
 * | 6  |(MSB)                                                                |
 * |----+--                        First storage element address            --|
 * | 7  |                                                                (LSB)|
 * |----+---------------------------------------------------------------------|
 * | 8  |(MSB)                                                                |
 * |----+--                        Number of storage elements               --|
 * | 9  |                                                                (LSB)|
 * |----+---------------------------------------------------------------------|
 * | 10 |(MSB)                                                                |
 * |----+--                        First import export element address      --|
 * | 11 |                                                                (LSB)|
 * |----+---------------------------------------------------------------------|
 * | 12 |(MSB)                                                                |
 * |----+--                        Number of import export elements         --|
 * | 13 |                                                                (LSB)|
 * |----+---------------------------------------------------------------------|
 * | 14 |(MSB)                                                                |
 * |----+--                        First data transfer element address      --|
 * | 15 |                                                                (LSB)|
 * |----+---------------------------------------------------------------------|
 * | 16 |(MSB)                                                                |
 * |----+--                        Number of data transfer elements         --|
 * | 17 |                                                                (LSB)|
 * |----+---------------------------------------------------------------------|
 * | 18 |                                                                     |
 * |----+--                        Reserved                                 --|
 * | 19 |                                                                     |
 * +==========================================================================+
 *
 *
 * The parameters savable (PS) bit is only used with the MODE SENSE
 * command.  This bit is reserved with the MODE SELECT command.  A PS bit
 * of one indicates that the target is capable of saving the page in a
 * non-volatile vendor-specific location.
 *
 * The first medium transport element address field identifies the first
 * medium transport element contained in the medium changer (other than
 * the default medium transport address of zero). The number of medium
 * transport elements field defines the total number of medium transport
 * elements contained in the medium changer. If the number of medium
 * transport elements field in a MODE SELECT command is greater than the
 * default value returned in the MODE SENSE parameter data, the target
 * shall return CHECK CONDITION status and set the sense key to ILLEGAL
 * REQUEST.
 *
 * The first storage element address field identifies the first medium
 * storage element contained in the medium changer.  The number of storage
 * elements field defines the total number of medium storage elements
 * contained in the medium changer. If the number of medium storage
 * elements field in a MODE SELECT command is greater than the default
 * value returned in the MODE SENSE parameter data, the target shall
 * return CHECK CONDITION status and set the sense key to ILLEGAL REQUEST.
 *
 * The first import export element address field identifies the first
 * medium portal that is accessible both by the medium transport devices
 * and also by an operator from outside the medium changer. The number of
 * import export elements field defines the total number of import export
 * elements contained in the medium changer and accessible to the medium
 * transport elements. If the number of import export elements field in a
 * MODE SELECT command is greater than the default value returned in the
 * MODE SENSE parameter data, the target shall return CHECK CONDITION
 * status and set the sense key to ILLEGAL REQUEST .
 *
 *    NOTE 207 The number of import export elements may be zero.
 *
 * The first data transfer element address field identifies the first data
 * transfer element contained in the medium changer.  The data transfer
 * elements may be either read/write or read-only devices.  The number of
 * data transfer field defines the total number of data transfer elements
 * contained within the medium changer and accessible to the medium
 * transport elements.  If the number of data transfer elements field in a
 * MODE SELECT command is greater than the default value returned in the
 * MODE SENSE parameter data, the target shall return CHECK CONDITION
 * status and set the sense key to ILLEGAL REQUEST .
 *
 * Each element in the medium changer must have a unique address. If the
 * address ranges defined for any of the element types overlap, the target
 * shall return CHECK CONDITION status and set the sense key to ILLEGAL
 * REQUEST.
 */

struct smc_raw_element_address_assignment_page {
  unsigned char page_code; /* 0x1D */
#define SMC_RAW_EA_PC_PS 0x80
  unsigned char param_length; /* 0x12 */
  unsigned char mte_addr[2];
  unsigned char mte_count[2];
  unsigned char se_addr[2];
  unsigned char se_count[2];
  unsigned char iee_addr[2];
  unsigned char iee_count[2];
  unsigned char dte_addr[2];
  unsigned char dte_count[2];
  unsigned char resv18[2];
};


#define SMC_GET2(VEC) \
  (uint16_t)((((unsigned char)(VEC)[0] << 8)) + (((unsigned char)(VEC)[1])))

#define SMC_PUT2(VEC, VAL) \
  ((VEC)[0] = (unsigned char)((VAL) >> 8), (VEC)[1] = (unsigned char)((VAL)))

#define SMC_GET3(VEC)                            \
  (uint32_t)((((unsigned char)(VEC)[0] << 16)) + \
             (((unsigned char)(VEC)[1] << 8)) + ((unsigned char)(VEC)[2]))

#define SMC_PUT3(VEC, VAL)                  \
  ((VEC)[0] = (unsigned char)((VAL) >> 16), \
   (VEC)[1] = (unsigned char)((VAL) >> 8), (VEC)[2] = (unsigned char)((VAL)))

#define SMC_GET4(VEC)                            \
  (uint32_t)((((unsigned char)(VEC)[0] << 24)) + \
             (((unsigned char)(VEC)[1] << 16)) + \
             (((unsigned char)(VEC)[2] << 8)) + ((unsigned char)(VEC)[3]))

#define SMC_PUT4(VEC, VAL)                  \
  ((VEC)[0] = (unsigned char)((VAL) >> 24), \
   (VEC)[1] = (unsigned char)((VAL) >> 16), \
   (VEC)[2] = (unsigned char)((VAL) >> 8), (VEC)[3] = (unsigned char)((VAL)))

#ifdef __cplusplus
}
#endif
