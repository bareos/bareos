/* Microsoft SQL Server Sample Code
 * Copyright (c) Microsoft Corporation
 *
 * All rights reserved.
 *
 * MIT License.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE. */

#ifndef BAREOS_WIN32_VDI_INCLUDE_VDIERROR_H_
#define BAREOS_WIN32_VDI_INCLUDE_VDIERROR_H_
/* bareos-check-sources:disable-copyright-check */
/* taken from:
 *   https://github.com/microsoft/sql-server-samples/
 *                             tree/master/samples/features/sqlvdi
 * added MIT license header
 * added bareos-style include guards
 * reformated in bareos style */

// original include guard.  Added here for safeties sake
#ifndef VDIERROR_H_
#  define VDIERROR_H_

//****************************************************************************
//		Copyright (c) Microsoft Corporation.
//
// vdierror.h
//
// Purpose:
//  Declare the error codes emitted by the virtual device interface.
//
//****************************************************************************

//---------------------------------------------------------------------------------------
// Error code handling will be done in standard COM fashion:
//
// an HRESULT is returned and the caller can use
// SUCCEEDED(code) or FAILED(code) to determine
// if the function failed or not.
//

// Form an error code mask.
// All VDI errors have 0x8077 as prefix.
//
#  define VD_ERROR(code) MAKE_HRESULT(SEVERITY_ERROR, 0x77, code)

// The object was not open
//
#  define VD_E_NOTOPEN VD_ERROR(2) /* 0x80770002 */

// The api was waiting and the timeout interval had elapsed.
//
#  define VD_E_TIMEOUT VD_ERROR(3) /* 0x80770003 */

// An abort request is preventing anything except termination actions.
//
#  define VD_E_ABORT VD_ERROR(4) /* 0x80770004 */

// Failed to create security environment.
//
#  define VD_E_SECURITY VD_ERROR(5) /* 0x80770005 */

// An invalid parameter was supplied
//
#  define VD_E_INVALID VD_ERROR(6) /* 0x80770006 */

// Failed to recognize the SQL Server instance name
//
#  define VD_E_INSTANCE_NAME VD_ERROR(7) /* 0x80770007 */

// The requested configuration is invalid
//
#  define VD_E_NOTSUPPORTED VD_ERROR(9) /* 0x80770009 */

// Out of memory
//
#  define VD_E_MEMORY VD_ERROR(10) /* 0x8077000a */

// Unexpected internal error
//
#  define VD_E_UNEXPECTED VD_ERROR(11) /* 0x8077000b */

// Protocol error
//
#  define VD_E_PROTOCOL VD_ERROR(12) /* 0x8077000c */

// All devices are open
//
#  define VD_E_OPEN VD_ERROR(13) /* 0x8077000d */

// the object is now closed
//
#  define VD_E_CLOSE VD_ERROR(14) /* 0x8077000e */

// the resource is busy
//
#  define VD_E_BUSY VD_ERROR(15) /* 0x8077000f */

#endif  // VDIERROR_H_
#endif  // BAREOS_WIN32_VDI_INCLUDE_VDIERROR_H_
