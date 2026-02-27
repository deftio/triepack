/*
 *   TITLE           :   dio_BitFile.h
 *   ORIGINAL AUTHOR :   M. A. Chatterjee
 *
 *   DESCRIPTION     :   Header File for BitFile functions.  Useful for
 *                       storing data in endian independant form and for
 *                       file serialization. 
 *
 **************** REVISION HISTORY ********************************************
 *
 *  Date        Author                Notes
 *  ==========  ======                ===================
 */

#ifndef  __dio_BitFile_h__
#define  __dio_BitFile_h__

#ifndef  __dio_Types_h__   /* type definitions e.g. u8 s8 ... */
#include "dio_Types.h"
#endif

#ifndef  __dio_Defs_h__    /* error definitions               */
#include "dio_Defs.h"
#endif  


#ifdef  __cplusplus
extern "C" {
#endif


typedef struct dio_BitFile dio_BitFile;

/************************************
 * A bit file object is defined here as a file of bits.  In order to have a
 * reasonably efficient addressing mechanism, the file of bits is controlled by
 * a position pointer, and a few simple read/write control functions, which can
 * place or extract integer values from the current position pointer.  If an attempt
 * to place an integer object at the end of the currently allocated file is
 * performed (e.g. the position pointer is at the EOF position) then this bitsteam
 * library will attempt to allocate more space will be performed.  If it is
 * desired to produce a single file of bits (for writing to a disk etc.) there are
 * two options.  The first is to read (as bytes) from the beginning of the bitsteam
 * to the end all of the bits (the last will be padded).  The second is to call the
 * function dio_BitFile_CreateBuffer(), which will create a single byte buffer 
 * (padded to the nearest byte if there are less than 8 bits in the last octect of 
 * the file) containing all of the bytes.
 * A note on Bit order.  In order to provide maximal flexibility on the bit order
 * used withn an octet, the following compile time definitions may be used:
 *  dio_BITFILE_BITORDER_BE  (MSb ... LSb = 76543210) 
 *  dio_BITFILE_BITORDER_LE  (MSb ... LSb = 01234567)
 */

/************************************
 * Default Constructor for bit file object
 * 
 * nAllocInc     : amount buffer grows (in bytes) when it needs to be resized
 */
dio_RESULT dio_BitFile_Construct (
						dio_BitFile** nppThis, 
						u8* npInitialBuffer,
						u32 nBufferSize,
						u32 nInitialBitPosition,  
						u32 nAllocIncSize,
						dio_BOOL nbMakeCopy
						);

/************************************
 * Destructor for bit file object
 */
dio_RESULT dio_BitFile_Destruct  (
						dio_BitFile** npThis
						);


/************************************
 * Take a series of bytes in a buffer and dump them in to the bitfile
 */
dio_RESULT dio_BitFile_AddBuffer ( 
						dio_BitFile *npThis,
						u8* npBuffer,
						u32 nBufferSizeInBits
						);

/************************************
 * Extract from a bitfile object, a buffer containing all of the bits
 */
dio_RESULT dio_BitFile_GetBuffer (
						dio_BitFile* npThis,
						u8** nppBuffer,
						u32* nBufferSizeInBits
						);

/************************************
 * Put a bit in to the bit file
 * if (nBit) 
 *    add a '1'
 * else
 *    add a '0'
 */
dio_RESULT dio_BitFile_PutBit (
						dio_BitFile* npThis, 
						u8 nBit
						);

/************************************
 * Put NBit quantity in to the file in to the bit file
 * for example suppose N is a 12 bit number contained in a
 * u32. Use dio_BitFile_PutNBitQty (pBitStream,N,12) to put
 * in a 12 bit quantity.
 */
dio_RESULT dio_BitFile_PutNBitQty (
						dio_BitFile* npThis, 
						u32 nNum, 
						u32 nNumBits);

/************************************
 *  Get a Bit at the current read position, see SetReadPosition
 *  also advances the read cursor one bit position
 *  Bit is in low order bit if nBit
 *  returns error if at end of file
 */
dio_RESULT dio_BitFile_GetBit (
						dio_BitFile* npThis, 
						u8* nBit
						);

/************************************
 * Get the nBit Qty  from the file at the current read position
 * with optional Sign Extension
 */
dio_RESULT dio_BitFile_GetNBitQty (
						dio_BitFile* npThis, 
						s32* npNum, 
						u32 nNumBits,
						dio_BOOL nbSignExtend
						);

/************************************
 * Set the position index (in bits) for Get and Put ... calls
 * returns error if the position is greater than internal buf size (in bits)
 */
dio_RESULT dio_BitFile_SetPosition (
						dio_BitFile* npThis, 
						u32 nBitPosition
						);


/************************************
 * Get the position index (in bits) for Get and Put ... calls
 */
dio_RESULT dio_BitFile_GetPosition (
						dio_BitFile* npThis, 
						u32* npBitPosition
						);


/************************************
 * Get the size of the bitsteam (in bits) 
 * Note that this necessarily means that the number of bytes needed 
 * to hold a buffer of bits would be:
 * NumBytesNeeded == 
 *    (NumBitsInStream >> 3) + ((NumBitsInStream&7)?1:0);
 */
dio_RESULT dio_BitFile_GetBitFileSize (
						dio_BitFile* npThis, 
						u32* npNumBitsInStream
						);

/************************************
 * Treat the specified Byte pointer as a BitFile and read at the 
 * position (in bits) specified
 */
dio_RESULT dio_BitFile_ReadAsBitFileNBitQtyAt(
						u8* npBytes,
						s32* npNum,
						u32 nNumBits, 
						u32 nBitPosition,
						dio_BOOL nbSignExtend
						);

#ifdef  __cplusplus
}
#endif

#endif /* __dio_BitFile_h__ */