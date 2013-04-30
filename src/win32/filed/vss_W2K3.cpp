/* 
 * We need one class per OS as Microsofts API 
 * differs only by calling convention and some
 * function we don't use.
 *
 * vss_generic will handle all versions and
 * switch between different headers to include.
*/

#ifdef WIN32_VSS
#define B_VSS_W2K3
#include "vss_generic.cpp"
#endif
